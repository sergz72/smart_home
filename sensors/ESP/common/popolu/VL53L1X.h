#ifndef _VL53L1X_H_
#define _VL53L1X_H_

typedef enum { Short, Medium, Long } DistanceMode;

typedef enum
{
  RangeValid                =   0,

  // "sigma estimator check is above the internal defined threshold"
  // (sigma = standard deviation of measurement)
  SigmaFail                 =   1,

  // "signal value is below the internal defined threshold"
  SignalFail                =   2,

  // "Target is below minimum detection threshold."
  RangeValidMinRangeClipped =   3,

  // "phase is out of bounds"
  // (nothing detected in range; try a longer distance mode if applicable)
  OutOfBoundsFail           =   4,

  // "HW or VCSEL failure"
  HardwareFail              =   5,

  // "The Range is valid but the wraparound check has not been done."
  RangeValidNoWrapCheckFail =   6,

  // "Wrapped target, not matching phases"
  // "no matching phase in other VCSEL period timing."
  WrapTargetFail            =   7,

  // "Internal algo underflow or overflow in lite ranging."
// ProcessingFail            =   8: not used in API

  // "Specific to lite ranging."
  // should never occur with this lib (which uses low power auto ranging,
  // as the API does)
  XtalkSignalFail           =   9,

  // "1st interrupt when starting ranging in back to back mode. Ignore
  // data."
  // should never occur with this lib
  SynchronizationInt         =  10, // (the API spells this "syncronisation")

  // "All Range ok but object is result of multiple pulses merging together.
  // Used by RQL for merged pulse detection"
// RangeValid MergedPulse    =  11: not used in API

  // "Used by RQL as different to phase fail."
// TargetPresentLackOfSignal =  12:

  // "Target is below minimum detection threshold."
  MinRangeFail              =  13,

  // "The reported range is invalid"
// RangeInvalid              =  14: can't actually be returned by API (range can never become negative, even after correction)

  // "No Update."
  None                      = 255,
} RangeStatus;

typedef struct
{
  unsigned short range_mm;
  RangeStatus range_status;
  float peak_signal_count_rate_MCPS;
  float ambient_count_rate_MCPS;
} RangingData;

int VL53L1X_init(int io_2v8, DistanceMode dm, int timingBudget, unsigned int period_ms);
const char * VL53L1X_rangeStatusToString(RangeStatus status);
int VL53L1X_startContinuous(void);
int VL53L1X_stopContinuous(void);
int VL53L1X_readSingle(RangingData *d);
int VL53L1X_read(RangingData *result);

#endif
