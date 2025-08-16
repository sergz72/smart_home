#include <VL53L1X.h>
#include "driver/i2c.h"
#include <board.h>
#include <common.h>

#define I2C_MASTER_NUM              0
#define I2C_MASTER_TIMEOUT_MS       1000

// register addresses from API vl53l1x_register_map.h
typedef enum
{
  SOFT_RESET                                                                 = 0x0000,
  OSC_MEASURED__FAST_OSC__FREQUENCY                                          = 0x0006,
  VHV_CONFIG__TIMEOUT_MACROP_LOOP_BOUND                                      = 0x0008,
  VHV_CONFIG__INIT                                                           = 0x000B,
  ALGO__PART_TO_PART_RANGE_OFFSET_MM                                         = 0x001E,
  MM_CONFIG__OUTER_OFFSET_MM                                                 = 0x0022,
  DSS_CONFIG__TARGET_TOTAL_RATE_MCPS                                         = 0x0024,
  PAD_I2C_HV__EXTSUP_CONFIG                                                  = 0x002E,
  GPIO__TIO_HV_STATUS                                                        = 0x0031,
  SIGMA_ESTIMATOR__EFFECTIVE_PULSE_WIDTH_NS                                  = 0x0036,
  SIGMA_ESTIMATOR__EFFECTIVE_AMBIENT_WIDTH_NS                                = 0x0037,
  ALGO__CROSSTALK_COMPENSATION_VALID_HEIGHT_MM                               = 0x0039,
  ALGO__RANGE_IGNORE_VALID_HEIGHT_MM                                         = 0x003E,
  ALGO__RANGE_MIN_CLIP                                                       = 0x003F,
  ALGO__CONSISTENCY_CHECK__TOLERANCE                                         = 0x0040,
  CAL_CONFIG__VCSEL_START                                                    = 0x0047,
  PHASECAL_CONFIG__TIMEOUT_MACROP                                            = 0x004B,
  PHASECAL_CONFIG__OVERRIDE                                                  = 0x004D,
  DSS_CONFIG__ROI_MODE_CONTROL                                               = 0x004F,
  SYSTEM__THRESH_RATE_HIGH                                                   = 0x0050,
  SYSTEM__THRESH_RATE_LOW                                                    = 0x0052,
  DSS_CONFIG__MANUAL_EFFECTIVE_SPADS_SELECT                                  = 0x0054,
  DSS_CONFIG__APERTURE_ATTENUATION                                           = 0x0057,
  MM_CONFIG__TIMEOUT_MACROP_A                                                = 0x005A, // added by Pololu for 16-bit accesses
  MM_CONFIG__TIMEOUT_MACROP_B                                                = 0x005C, // added by Pololu for 16-bit accesses
  RANGE_CONFIG__TIMEOUT_MACROP_A                                             = 0x005E, // added by Pololu for 16-bit accesses
  RANGE_CONFIG__VCSEL_PERIOD_A                                               = 0x0060,
  RANGE_CONFIG__TIMEOUT_MACROP_B                                             = 0x0061, // added by Pololu for 16-bit accesses
  RANGE_CONFIG__VCSEL_PERIOD_B                                               = 0x0063,
  RANGE_CONFIG__SIGMA_THRESH                                                 = 0x0064,
  RANGE_CONFIG__MIN_COUNT_RATE_RTN_LIMIT_MCPS                                = 0x0066,
  RANGE_CONFIG__VALID_PHASE_HIGH                                             = 0x0069,
  SYSTEM__INTERMEASUREMENT_PERIOD                                            = 0x006C,
  SYSTEM__GROUPED_PARAMETER_HOLD_0                                           = 0x0071,
  SYSTEM__SEED_CONFIG                                                        = 0x0077,
  SD_CONFIG__WOI_SD0                                                         = 0x0078,
  SD_CONFIG__WOI_SD1                                                         = 0x0079,
  SD_CONFIG__INITIAL_PHASE_SD0                                               = 0x007A,
  SD_CONFIG__INITIAL_PHASE_SD1                                               = 0x007B,
  SYSTEM__GROUPED_PARAMETER_HOLD_1                                           = 0x007C,
  SD_CONFIG__QUANTIFIER                                                      = 0x007E,
  SYSTEM__SEQUENCE_CONFIG                                                    = 0x0081,
  SYSTEM__GROUPED_PARAMETER_HOLD                                             = 0x0082,
  SYSTEM__INTERRUPT_CLEAR                                                    = 0x0086,
  SYSTEM__MODE_START                                                         = 0x0087,
  RESULT__RANGE_STATUS                                                       = 0x0089,
  PHASECAL_RESULT__VCSEL_START                                               = 0x00D8,
  RESULT__OSC_CALIBRATE_VAL                                                  = 0x00DE,
  FIRMWARE__SYSTEM_STATUS                                                    = 0x00E5,
  IDENTIFICATION__MODEL_ID                                                   = 0x010F,
} regAddr;

typedef struct
{
  unsigned char range_status;
  // unsigned char report_status: not used
  unsigned char stream_count;
  unsigned short dss_actual_effective_spads_sd0;
  // unsigned short peak_signal_count_rate_mcps_sd0: not used
  unsigned short ambient_count_rate_mcps_sd0;
  // unsigned short sigma_sd0: not used
  // unsigned short phase_sd0: not used
  unsigned short final_crosstalk_corrected_range_mm_sd0;
  unsigned short peak_signal_count_rate_crosstalk_corrected_mcps_sd0;
} ResultBuffer;

// value used in measurement timing budget calculations
// assumes PresetMode is LOWPOWER_AUTONOMOUS
//
// vhv = LOWPOWER_AUTO_VHV_LOOP_DURATION_US + LOWPOWERAUTO_VHV_LOOP_BOUND
//       (tuning parm default) * LOWPOWER_AUTO_VHV_LOOP_DURATION_US
//     = 245 + 3 * 245 = 980
// TimingGuard = LOWPOWER_AUTO_OVERHEAD_BEFORE_A_RANGING +
//               LOWPOWER_AUTO_OVERHEAD_BETWEEN_A_B_RANGING + vhv
//             = 1448 + 2100 + 980 = 4528
#define TimingGuard 4528

// value in DSS_CONFIG__TARGET_TOTAL_RATE_MCPS register, used in DSS
// calculations
#define TargetRate 0x0A00

#ifndef IO_TIMEOUT
#define IO_TIMEOUT 10 // ms
#endif

static unsigned short fast_osc_frequency;
static unsigned short osc_calibrate_val;
static unsigned long long int timeout_start_ms;
static int calibrated;
unsigned char saved_vhv_init;
unsigned char saved_vhv_timeout;

static int VL53L1_ReadMulti(regAddr index, unsigned char *pdata, unsigned int count)
{
  unsigned char data[2];
  data[0] = index >> 8;
  data[1] = index & 0xFF;
  return i2c_master_write_read_device(I2C_MASTER_NUM, VL53L1_SENSOR_ADDR, data,
                                              2, pdata, count, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

static int readReg(regAddr reg, unsigned char *rdata)
{
  unsigned char data[2];
  data[0] = reg >> 8;
  data[1] = reg & 0xFF;
  return i2c_master_write_read_device(I2C_MASTER_NUM, VL53L1_SENSOR_ADDR, data,
                                              2, rdata, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

static unsigned short readReg16Bit(regAddr reg, unsigned short *odata)
{
  unsigned char data[2], rdata[2];
  int rc;
  data[0] = reg >> 8;
  data[1] = reg & 0xFF;
  rc = i2c_master_write_read_device(I2C_MASTER_NUM, VL53L1_SENSOR_ADDR, data,
                                            2, rdata, 2, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
  if (!rc)
    *odata = ((uint16_t)rdata[0] << 8) | rdata[1];
  return rc;
}

static int writeReg(regAddr reg, unsigned char value)
{
  unsigned char data[3];
  data[0] = reg >> 8;
  data[1] = reg & 0xFF;
  data[2] = value;
  return i2c_master_write_to_device(I2C_MASTER_NUM, VL53L1_SENSOR_ADDR, data, 3,
                                            I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

static int writeReg16Bit(regAddr reg, unsigned short value)
{
  unsigned char data[4];
  data[0] = reg >> 8;
  data[1] = reg & 0xFF;
  data[2] = value >> 8;
  data[3] = value & 0xFF;
  return i2c_master_write_to_device(I2C_MASTER_NUM, VL53L1_SENSOR_ADDR, data, 4,
                                            I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

static int writeReg32Bit(regAddr reg, unsigned int value)
{
  unsigned char data[6];
  data[0] = reg >> 8;
  data[1] = reg & 0xFF;
  data[2] = value >> 24;
  data[3] = (value >> 16) & 0xFF;
  data[4] = (value >> 8) & 0xFF;
  data[5] = value & 0xFF;
  return i2c_master_write_to_device(I2C_MASTER_NUM, VL53L1_SENSOR_ADDR, data, 6,
                                            I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

// Record the current time to check an upcoming timeout against
static void VL53L1X_startTimeout() { timeout_start_ms = get_time_ms(); }

// Check if timeout is enabled (set to nonzero value) and has expired
static int VL53L1X_checkTimeoutExpired() { return get_time_ms() - timeout_start_ms > IO_TIMEOUT; }

// Calculate macro period in microseconds (12.12 format) with given VCSEL period
// assumes fast_osc_frequency has been read and stored
// based on VL53L1_calc_macro_period_us()
static unsigned int VL53L1X_calcMacroPeriod(unsigned char vcsel_period)
{
  // from VL53L1_calc_pll_period_us()
  // fast osc frequency in 4.12 format; PLL period in 0.24 format
  unsigned int pll_period_us = ((unsigned int)0x01 << 30) / fast_osc_frequency;

  // from VL53L1_decode_vcsel_period()
  unsigned char vcsel_period_pclks = (vcsel_period + 1) << 1;

  // VL53L1_MACRO_PERIOD_VCSEL_PERIODS = 2304
  unsigned int macro_period_us = (unsigned int)2304 * pll_period_us;
  macro_period_us >>= 6;
  macro_period_us *= vcsel_period_pclks;
  macro_period_us >>= 6;

  return macro_period_us;
}

// Convert sequence step timeout from microseconds to macro periods with given
// macro period in microseconds (12.12 format)
// based on VL53L1_calc_timeout_mclks()
static unsigned int VL53L1X_timeoutMicrosecondsToMclks(unsigned int timeout_us, unsigned int macro_period_us)
{
  return (((unsigned int)timeout_us << 12) + (macro_period_us >> 1)) / macro_period_us;
}

// Encode sequence step timeout register value from timeout in MCLKs
// based on VL53L1_encode_timeout()
static unsigned short VL53L1X_encodeTimeout(unsigned int timeout_mclks)
{
  // encoded format: "(LSByte * 2^MSByte) + 1"

  unsigned int ls_byte = 0;
  unsigned short ms_byte = 0;

  if (timeout_mclks > 0)
  {
    ls_byte = timeout_mclks - 1;

    while ((ls_byte & 0xFFFFFF00) > 0)
    {
      ls_byte >>= 1;
      ms_byte++;
    }

    return (ms_byte << 8) | (ls_byte & 0xFF);
  }
  else { return 0; }
}

// Set the measurement timing budget in microseconds, which is the time allowed
// for one measurement. A longer timing budget allows for more accurate
// measurements.
// based on VL53L1_SetMeasurementTimingBudgetMicroSeconds()
static int VL53L1X_setMeasurementTimingBudget(unsigned int budget_us)
{
  unsigned char v;
  int rc;
  // assumes PresetMode is LOWPOWER_AUTONOMOUS

  if (budget_us <= TimingGuard) { return 1; }

  unsigned int range_config_timeout_us = budget_us -= TimingGuard;
  if (range_config_timeout_us > 1100000) { return 1; } // FDA_MAX_TIMING_BUDGET_US * 2

  range_config_timeout_us /= 2;

  // VL53L1_calc_timeout_register_values() begin

  unsigned int macro_period_us;

  rc = readReg(RANGE_CONFIG__VCSEL_PERIOD_A, &v);
  if (rc)
    return rc;
  // "Update Macro Period for Range A VCSEL Period"
  macro_period_us = VL53L1X_calcMacroPeriod(v);

  // "Update Phase timeout - uses Timing A"
  // Timeout of 1000 is tuning parm default (TIMED_PHASECAL_CONFIG_TIMEOUT_US_DEFAULT)
  // via VL53L1_get_preset_mode_timing_cfg().
  unsigned int phasecal_timeout_mclks = VL53L1X_timeoutMicrosecondsToMclks(1000, macro_period_us);
  if (phasecal_timeout_mclks > 0xFF) { phasecal_timeout_mclks = 0xFF; }
  rc = writeReg(PHASECAL_CONFIG__TIMEOUT_MACROP, phasecal_timeout_mclks);
  if (rc)
    return rc;

  // "Update MM Timing A timeout"
  // Timeout of 1 is tuning parm default (LOWPOWERAUTO_MM_CONFIG_TIMEOUT_US_DEFAULT)
  // via VL53L1_get_preset_mode_timing_cfg(). With the API, the register
  // actually ends up with a slightly different value because it gets assigned,
  // retrieved, recalculated with a different macro period, and reassigned,
  // but it probably doesn't matter because it seems like the MM ("mode
  // mitigation"?) sequence steps are disabled in low power auto mode anyway.
  rc = writeReg16Bit(MM_CONFIG__TIMEOUT_MACROP_A, VL53L1X_encodeTimeout(
      VL53L1X_timeoutMicrosecondsToMclks(1, macro_period_us)));
  if (rc)
    return rc;

  // "Update Range Timing A timeout"
  rc = writeReg16Bit(RANGE_CONFIG__TIMEOUT_MACROP_A, VL53L1X_encodeTimeout(
      VL53L1X_timeoutMicrosecondsToMclks(range_config_timeout_us, macro_period_us)));
  if (rc)
    return rc;

  rc = readReg(RANGE_CONFIG__VCSEL_PERIOD_B, &v);
  if (rc)
    return rc;
  // "Update Macro Period for Range B VCSEL Period"
  macro_period_us = VL53L1X_calcMacroPeriod(v);

  // "Update MM Timing B timeout"
  // (See earlier comment about MM Timing A timeout.)
  rc = writeReg16Bit(MM_CONFIG__TIMEOUT_MACROP_B, VL53L1X_encodeTimeout(
      VL53L1X_timeoutMicrosecondsToMclks(1, macro_period_us)));
  if (rc)
    return rc;

  // "Update Range Timing B timeout"
  return writeReg16Bit(RANGE_CONFIG__TIMEOUT_MACROP_B, VL53L1X_encodeTimeout(
      VL53L1X_timeoutMicrosecondsToMclks(range_config_timeout_us, macro_period_us)));

  // VL53L1_calc_timeout_register_values() end
}

// set distance mode to Short, Medium, or Long
// based on VL53L1_SetDistanceMode()
static int VL53L1X_setDistanceMode(DistanceMode mode, unsigned int timingBudget)
{
  int rc;

  switch (mode)
  {
    case Short:
      // from VL53L1_preset_mode_standard_ranging_short_range()

      // timing config
      rc = writeReg(RANGE_CONFIG__VCSEL_PERIOD_A, 0x07);
      if (rc)
        return rc;
      rc = writeReg(RANGE_CONFIG__VCSEL_PERIOD_B, 0x05);
      if (rc)
        return rc;
      rc = writeReg(RANGE_CONFIG__VALID_PHASE_HIGH, 0x38);
      if (rc)
        return rc;

      // dynamic config
      rc = writeReg(SD_CONFIG__WOI_SD0, 0x07);
      if (rc)
        return rc;
      rc = writeReg(SD_CONFIG__WOI_SD1, 0x05);
      if (rc)
        return rc;
      rc = writeReg(SD_CONFIG__INITIAL_PHASE_SD0, 6); // tuning parm default
      if (rc)
        return rc;
      rc = writeReg(SD_CONFIG__INITIAL_PHASE_SD1, 6); // tuning parm default
      if (rc)
        return rc;

      break;

    case Medium:
      // from VL53L1_preset_mode_standard_ranging()

      // timing config
      rc = writeReg(RANGE_CONFIG__VCSEL_PERIOD_A, 0x0B);
      if (rc)
        return rc;
      rc = writeReg(RANGE_CONFIG__VCSEL_PERIOD_B, 0x09);
      if (rc)
        return rc;
      rc = writeReg(RANGE_CONFIG__VALID_PHASE_HIGH, 0x78);
      if (rc)
        return rc;

      // dynamic config
      rc = writeReg(SD_CONFIG__WOI_SD0, 0x0B);
      if (rc)
        return rc;
      rc = writeReg(SD_CONFIG__WOI_SD1, 0x09);
      if (rc)
        return rc;
      rc = writeReg(SD_CONFIG__INITIAL_PHASE_SD0, 10); // tuning parm default
      if (rc)
        return rc;
      rc = writeReg(SD_CONFIG__INITIAL_PHASE_SD1, 10); // tuning parm default
      if (rc)
        return rc;

      break;

    case Long: // long
      // from VL53L1_preset_mode_standard_ranging_long_range()

      // timing config
      rc = writeReg(RANGE_CONFIG__VCSEL_PERIOD_A, 0x0F);
      if (rc)
        return rc;
      rc = writeReg(RANGE_CONFIG__VCSEL_PERIOD_B, 0x0D);
      if (rc)
        return rc;
      rc = writeReg(RANGE_CONFIG__VALID_PHASE_HIGH, 0xB8);
      if (rc)
        return rc;

      // dynamic config
      rc = writeReg(SD_CONFIG__WOI_SD0, 0x0F);
      if (rc)
        return rc;
      rc = writeReg(SD_CONFIG__WOI_SD1, 0x0D);
      if (rc)
        return rc;
      rc = writeReg(SD_CONFIG__INITIAL_PHASE_SD0, 14); // tuning parm default
      if (rc)
        return rc;
      rc = writeReg(SD_CONFIG__INITIAL_PHASE_SD1, 14); // tuning parm default
      if (rc)
        return rc;

      break;

    default:
      // unrecognized mode - do nothing
      return 1;
  }

  // reapply timing budget
  return VL53L1X_setMeasurementTimingBudget(timingBudget);
}

int VL53L1X_init(int io_2v8, DistanceMode dm, int timingBudget, unsigned int period_ms)
{
  int rc;
  unsigned short v16 = 0;
  unsigned char v;

  calibrated = 0;
  saved_vhv_init = saved_vhv_timeout = 0;

  rc = readReg16Bit(IDENTIFICATION__MODEL_ID, &v16);
  if (rc)
    return rc;
  // check model ID and module type registers (values specified in datasheet)
  if (v16 != 0xEACC) { return v16; }

  // VL53L1_poll_for_boot_completion() begin

  VL53L1X_startTimeout();

  // check last_status in case we still get a NACK to try to deal with it correctly
  for (;;)
  {
    rc = readReg(FIRMWARE__SYSTEM_STATUS, &v);
    if (rc)
      return rc;
    if (v & 0x01)
      break;
    if (VL53L1X_checkTimeoutExpired())
      return 1;
  }
  // VL53L1_poll_for_boot_completion() end

  // VL53L1_software_reset() end

  // VL53L1_DataInit() begin

  // sensor uses 1V8 mode for I/O by default; switch to 2V8 mode if necessary
  if (io_2v8)
  {
    rc = readReg(PAD_I2C_HV__EXTSUP_CONFIG, &v);
    if (rc)
      return rc;
    rc = writeReg(PAD_I2C_HV__EXTSUP_CONFIG, v | 0x01);
    if (rc)
      return rc;
  }

  // store oscillator info for later use
  rc = readReg16Bit(OSC_MEASURED__FAST_OSC__FREQUENCY, &fast_osc_frequency);
  if (rc)
    return rc;
  rc = readReg16Bit(RESULT__OSC_CALIBRATE_VAL, &osc_calibrate_val);
  if (rc)
    return rc;

  // VL53L1_DataInit() end

  // VL53L1_StaticInit() begin

  // Note that the API does not actually apply the configuration settings below
  // when VL53L1_StaticInit() is called: it keeps a copy of the sensor's
  // register contents in memory and doesn't actually write them until a
  // measurement is started. Writing the configuration here means we don't have
  // to keep it all in memory and avoids a lot of redundant writes later.

  // the API sets the preset mode to LOWPOWER_AUTONOMOUS here:
  // VL53L1_set_preset_mode() begin

  // VL53L1_preset_mode_standard_ranging() begin

  // values labeled "tuning parm default" are from vl53l1_tuning_parm_defaults.h
  // (API uses these in VL53L1_init_tuning_parm_storage_struct())

  // static config
  // API resets PAD_I2C_HV__EXTSUP_CONFIG here, but maybe we don't want to do
  // that? (seems like it would disable 2V8 mode)
  rc = writeReg16Bit(DSS_CONFIG__TARGET_TOTAL_RATE_MCPS, TargetRate); // should already be this value after reset
  if (rc)
    return rc;
  rc = writeReg(GPIO__TIO_HV_STATUS, 0x02);
  if (rc)
    return rc;
  rc = writeReg(SIGMA_ESTIMATOR__EFFECTIVE_PULSE_WIDTH_NS, 8); // tuning parm default
  if (rc)
    return rc;
  rc = writeReg(SIGMA_ESTIMATOR__EFFECTIVE_AMBIENT_WIDTH_NS, 16); // tuning parm default
  if (rc)
    return rc;
  rc = writeReg(ALGO__CROSSTALK_COMPENSATION_VALID_HEIGHT_MM, 0x01);
  if (rc)
    return rc;
  rc = writeReg(ALGO__RANGE_IGNORE_VALID_HEIGHT_MM, 0xFF);
  if (rc)
    return rc;
  rc = writeReg(ALGO__RANGE_MIN_CLIP, 0); // tuning parm default
  if (rc)
    return rc;
  rc = writeReg(ALGO__CONSISTENCY_CHECK__TOLERANCE, 2); // tuning parm default
  if (rc)
    return rc;

  // general config
  rc = writeReg16Bit(SYSTEM__THRESH_RATE_HIGH, 0x0000);
  if (rc)
    return rc;
  rc = writeReg16Bit(SYSTEM__THRESH_RATE_LOW, 0x0000);
  if (rc)
    return rc;
  rc = writeReg(DSS_CONFIG__APERTURE_ATTENUATION, 0x38);
  if (rc)
    return rc;

  // timing config
  // most of these settings will be determined later by distance and timing
  // budget configuration
  rc = writeReg16Bit(RANGE_CONFIG__SIGMA_THRESH, 360); // tuning parm default
  if (rc)
    return rc;
  rc = writeReg16Bit(RANGE_CONFIG__MIN_COUNT_RATE_RTN_LIMIT_MCPS, 192); // tuning parm default
  if (rc)
    return rc;

  // dynamic config

  rc = writeReg(SYSTEM__GROUPED_PARAMETER_HOLD_0, 0x01);
  if (rc)
    return rc;
  rc = writeReg(SYSTEM__GROUPED_PARAMETER_HOLD_1, 0x01);
  if (rc)
    return rc;
  rc = writeReg(SD_CONFIG__QUANTIFIER, 2); // tuning parm default
  if (rc)
    return rc;

  // VL53L1_preset_mode_standard_ranging() end

  // from VL53L1_preset_mode_timed_ranging_*
  // GPH is 0 after reset, but writing GPH0 and GPH1 above seem to set GPH to 1,
  // and things don't seem to work if we don't set GPH back to 0 (which the API
  // does here).
  rc = writeReg(SYSTEM__GROUPED_PARAMETER_HOLD, 0x00);
  if (rc)
    return rc;
  rc = writeReg(SYSTEM__SEED_CONFIG, 1); // tuning parm default
  if (rc)
    return rc;

  // from VL53L1_config_low_power_auto_mode
  rc = writeReg(SYSTEM__SEQUENCE_CONFIG, 0x8B); // VHV, PHASECAL, DSS1, RANGE
  if (rc)
    return rc;
  rc = writeReg16Bit(DSS_CONFIG__MANUAL_EFFECTIVE_SPADS_SELECT, 200 << 8);
  if (rc)
    return rc;
  rc = writeReg(DSS_CONFIG__ROI_MODE_CONTROL, 2); // REQUESTED_EFFFECTIVE_SPADS
  if (rc)
    return rc;

  // VL53L1_set_preset_mode() end

  // default to long range, 50 ms timing budget
  // note that this is different than what the API defaults to
  rc = VL53L1X_setDistanceMode(dm, timingBudget);
  if (rc)
    return rc;

  // VL53L1_StaticInit() end

  rc = readReg16Bit(MM_CONFIG__OUTER_OFFSET_MM, &v16);
  if (rc)
    return rc;
  // the API triggers this change in VL53L1_init_and_start_range() once a
  // measurement is started; assumes MM1 and MM2 are disabled
  rc = writeReg16Bit(ALGO__PART_TO_PART_RANGE_OFFSET_MM,
                v16 * 4);
  if (rc)
    return rc;

  // from VL53L1_set_inter_measurement_period_ms()
  return writeReg32Bit(SYSTEM__INTERMEASUREMENT_PERIOD, period_ms * osc_calibrate_val);
}

// "Setup ranges after the first one in low power auto mode by turning off
// FW calibration steps and programming static values"
// based on VL53L1_low_power_auto_setup_manual_calibration()
static int VL53L1X_setupManualCalibration()
{
  int rc;
  unsigned char v;

  // "save original vhv configs"
  rc = readReg(VHV_CONFIG__INIT, &saved_vhv_init);
  if (rc)
    return rc;
  rc = readReg(VHV_CONFIG__TIMEOUT_MACROP_LOOP_BOUND, &saved_vhv_timeout);
  if (rc)
    return rc;

  // "disable VHV init"
  rc = writeReg(VHV_CONFIG__INIT, saved_vhv_init & 0x7F);
  if (rc)
    return rc;

  // "set loop bound to tuning param"
  rc = writeReg(VHV_CONFIG__TIMEOUT_MACROP_LOOP_BOUND,
           (saved_vhv_timeout & 0x03) + (3 << 2)); // tuning parm default (LOWPOWERAUTO_VHV_LOOP_BOUND_DEFAULT)
  if (rc)
    return rc;

  // "override phasecal"
  rc = writeReg(PHASECAL_CONFIG__OVERRIDE, 0x01);
  if (rc)
    return rc;
  rc = readReg(PHASECAL_RESULT__VCSEL_START, &v);
  if (rc)
    return rc;
  return writeReg(CAL_CONFIG__VCSEL_START, v);
}

// read measurement results into buffer
static int VL53L1X_readResults(ResultBuffer *results)
{
  int rc;
  unsigned char Temp[17];

  rc = VL53L1_ReadMulti(RESULT__RANGE_STATUS, Temp, 17);
  if (rc)
    return rc;

  results->range_status = Temp[0];

  //bus->read(); // report_status: not used

  results->stream_count = Temp[2];

  results->dss_actual_effective_spads_sd0  = (unsigned short)Temp[3] << 8 | Temp[4]; // high byte

  //bus->read(); // peak_signal_count_rate_mcps_sd0: not used
  //bus->read();

  results->ambient_count_rate_mcps_sd0  = (unsigned short)Temp[7] << 8 | Temp[8];

  //bus->read(); // sigma_sd0: not used
  //bus->read();

  //bus->read(); // phase_sd0: not used
  //bus->read();

  results->final_crosstalk_corrected_range_mm_sd0  = (unsigned short)Temp[13] << 8 | Temp[14];

  results->peak_signal_count_rate_crosstalk_corrected_mcps_sd0  = (unsigned short)Temp[15] << 8 | Temp[16];

  return 0;
}

// perform Dynamic SPAD Selection calculation/update
// based on VL53L1_low_power_auto_update_DSS()
static int VL53L1X_updateDSS(ResultBuffer *results)
{
  unsigned short spadCount = results->dss_actual_effective_spads_sd0;

  if (spadCount != 0)
  {
    // "Calc total rate per spad"

    unsigned int totalRatePerSpad =
        (unsigned int)results->peak_signal_count_rate_crosstalk_corrected_mcps_sd0 +
        results->ambient_count_rate_mcps_sd0;

    // "clip to 16 bits"
    if (totalRatePerSpad > 0xFFFF) { totalRatePerSpad = 0xFFFF; }

    // "shift up to take advantage of 32 bits"
    totalRatePerSpad <<= 16;

    totalRatePerSpad /= spadCount;

    if (totalRatePerSpad != 0)
    {
      // "get the target rate and shift up by 16"
      unsigned int requiredSpads = ((unsigned int)TargetRate << 16) / totalRatePerSpad;

      // "clip to 16 bit"
      if (requiredSpads > 0xFFFF) { requiredSpads = 0xFFFF; }

      // "override DSS config"
      return writeReg16Bit(DSS_CONFIG__MANUAL_EFFECTIVE_SPADS_SELECT, requiredSpads);
      // DSS_CONFIG__ROI_MODE_CONTROL should already be set to REQUESTED_EFFFECTIVE_SPADS
    }
  }

  // If we reached this point, it means something above would have resulted in a
  // divide by zero.
  // "We want to gracefully set a spad target, not just exit with an error"

  // "set target to mid point"
  return writeReg16Bit(DSS_CONFIG__MANUAL_EFFECTIVE_SPADS_SELECT, 0x8000);
}

// Convert count rate from fixed point 9.7 format to float
static float countRateFixedToFloat(unsigned short count_rate_fixed) { return (float)count_rate_fixed / (1 << 7); }

// get range, status, rates from results buffer
// based on VL53L1_GetRangingMeasurementData()
static void VL53L1X_getRangingData(ResultBuffer *results, RangingData *ranging_data)
{
  // VL53L1_copy_sys_and_core_results_to_range_results() begin

  unsigned short range = results->final_crosstalk_corrected_range_mm_sd0;

  // "apply correction gain"
  // gain factor of 2011 is tuning parm default (VL53L1_TUNINGPARM_LITE_RANGING_GAIN_FACTOR_DEFAULT)
  // Basically, this appears to scale the result by 2011/2048, or about 98%
  // (with the 1024 added for proper rounding).
  ranging_data->range_mm = ((unsigned int)range * 2011 + 0x0400) / 0x0800;

  // VL53L1_copy_sys_and_core_results_to_range_results() end

  // set range_status in ranging_data based on value of RESULT__RANGE_STATUS register
  // mostly based on ConvertStatusLite()
  switch(results->range_status)
  {
    case 17: // MULTCLIPFAIL
    case 2: // VCSELWATCHDOGTESTFAILURE
    case 1: // VCSELCONTINUITYTESTFAILURE
    case 3: // NOVHVVALUEFOUND
      // from SetSimpleData()
      ranging_data->range_status = HardwareFail;
      break;

    case 13: // USERROICLIP
      // from SetSimpleData()
      ranging_data->range_status = MinRangeFail;
      break;

    case 18: // GPHSTREAMCOUNT0READY
      ranging_data->range_status = SynchronizationInt;
      break;

    case 5: // RANGEPHASECHECK
      ranging_data->range_status =  OutOfBoundsFail;
      break;

    case 4: // MSRCNOTARGET
      ranging_data->range_status = SignalFail;
      break;

    case 6: // SIGMATHRESHOLDCHECK
      ranging_data->range_status = SigmaFail;
      break;

    case 7: // PHASECONSISTENCY
      ranging_data->range_status = WrapTargetFail;
      break;

    case 12: // RANGEIGNORETHRESHOLD
      ranging_data->range_status = XtalkSignalFail;
      break;

    case 8: // MINCLIP
      ranging_data->range_status = RangeValidMinRangeClipped;
      break;

    case 9: // RANGECOMPLETE
      // from VL53L1_copy_sys_and_core_results_to_range_results()
      if (results->stream_count == 0)
      {
        ranging_data->range_status = RangeValidNoWrapCheckFail;
      }
      else
      {
        ranging_data->range_status = RangeValid;
      }
      break;

    default:
      ranging_data->range_status = None;
  }

  // from SetSimpleData()
  ranging_data->peak_signal_count_rate_MCPS =
      countRateFixedToFloat(results->peak_signal_count_rate_crosstalk_corrected_mcps_sd0);
  ranging_data->ambient_count_rate_MCPS =
      countRateFixedToFloat(results->ambient_count_rate_mcps_sd0);
}

static int dataReady(int *dr) {
  unsigned char v;
  int rc = readReg(GPIO__TIO_HV_STATUS, &v);
  *dr = (v & 0x01) == 0;
  return rc;
}

// Returns a range reading in millimeters when continuous mode is active. If
// blocking is true (the default), this function waits for a new measurement to
// be available. If blocking is false, it will try to return data immediately.
// (readSingle() also calls this function after starting a single-shot range
// measurement)
int VL53L1X_read(RangingData *result)
{
  ResultBuffer b;
  int dr, rc;

  VL53L1X_startTimeout();
  for (;;)
  {
    rc = dataReady(&dr);
    if (rc)
      return rc;
    if (dr)
      break;
    if (VL53L1X_checkTimeoutExpired())
      return 2;
    delayms(1);
  }

  rc = VL53L1X_readResults(&b);
  if (rc)
    return rc;

  if (!calibrated)
  {
    rc = VL53L1X_setupManualCalibration();
    if (rc)
      return rc;
    calibrated = 1;
  }

  rc = VL53L1X_updateDSS(&b);
  if (rc)
    return rc;

  VL53L1X_getRangingData(&b, result);

  return writeReg(SYSTEM__INTERRUPT_CLEAR, 0x01); // sys_interrupt_clear_range
}

// Starts a single-shot range measurement. If blocking is true (the default),
// this function waits for the measurement to finish and returns the reading.
// Otherwise, it returns 0 immediately.
int VL53L1X_readSingle(RangingData *d)
{
  int rc = writeReg(SYSTEM__INTERRUPT_CLEAR, 0x01); // sys_interrupt_clear_range
  if (rc)
    return rc;
  rc = writeReg(SYSTEM__MODE_START, 0x10); // mode_range__single_shot
  if (rc)
    return rc;

  return VL53L1X_read(d);
}

// Start continuous ranging measurements, with the given inter-measurement
// period in milliseconds determining how often the sensor takes a measurement.
int VL53L1X_startContinuous(void)
{
  int rc = writeReg(SYSTEM__INTERRUPT_CLEAR, 0x01); // sys_interrupt_clear_range
  if (rc)
    return rc;
  return writeReg(SYSTEM__MODE_START, 0x40); // mode_range__timed
}

// Stop continuous measurements
// based on VL53L1_stop_range()
int VL53L1X_stopContinuous(void)
{
  int rc = writeReg(SYSTEM__MODE_START, 0x80); // mode_range__abort
  if (rc)
    return rc;

  // VL53L1_low_power_auto_data_stop_range() begin

  calibrated = 0;

  // "restore vhv configs"
  if (saved_vhv_init != 0)
  {
    rc = writeReg(VHV_CONFIG__INIT, saved_vhv_init);
    if (rc)
      return rc;
  }
  if (saved_vhv_timeout != 0)
  {
    rc = writeReg(VHV_CONFIG__TIMEOUT_MACROP_LOOP_BOUND, saved_vhv_timeout);
    if (rc)
      return rc;
  }

  // "remove phasecal override"
  return writeReg(PHASECAL_CONFIG__OVERRIDE, 0x00);

  // VL53L1_low_power_auto_data_stop_range() end
}

// convert a RangeStatus to a readable string
// Note that on an AVR, these strings are stored in RAM (dynamic memory), which
// makes working with them easier but uses up 200+ bytes of RAM (many AVR-based
// Arduinos only have about 2000 bytes of RAM). You can avoid this memory usage
// if you do not call this function in your sketch.
const char * VL53L1X_rangeStatusToString(RangeStatus status)
{
  switch (status)
  {
    case RangeValid:
      return "range valid";

    case SigmaFail:
      return "sigma fail";

    case SignalFail:
      return "signal fail";

    case RangeValidMinRangeClipped:
      return "range valid, min range clipped";

    case OutOfBoundsFail:
      return "out of bounds fail";

    case HardwareFail:
      return "hardware fail";

    case RangeValidNoWrapCheckFail:
      return "range valid, no wrap check fail";

    case WrapTargetFail:
      return "wrap target fail";

    case XtalkSignalFail:
      return "xtalk signal fail";

    case SynchronizationInt:
      return "synchronization int";

    case MinRangeFail:
      return "min range fail";

    case None:
      return "no update";

    default:
      return "unknown status";
  }
}
