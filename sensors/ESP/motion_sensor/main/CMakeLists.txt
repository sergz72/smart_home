idf_component_register(SRCS "../../common/main.c" "../../common/gatt_svr.c" "../../common/wifi.c" "../../common/ble.c" "../../common/nvs.c" "env.c" "../../common/led.c" "crypto.c" "udp_client.c"
  "../../common/ntp.c"
  "sht3x.c"
  "vl53l1/vl53l1_platform.c" "vl53l1/vl53l1x_api.c" "vl53l1/vl53l1x_calibration.c"
  INCLUDE_DIRS "." "./vl53l1" "../../common"
)
