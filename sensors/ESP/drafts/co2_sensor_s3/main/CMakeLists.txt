idf_component_register(
  SRCS "../../co2_sensor_common/main.c"
        "../../co2_sensor_common/hal.c"
        "../../co2_sensor_common/wifi_commands.c"
        "../../common/wifi.c"
        "../../../ARM_CLION/common_lib/getstring.c"
        "../../../ARM_CLION/common_lib/shell/shell.c"
        "../../../ARM_CLION/common_lib/fixed_queue.c"
  INCLUDE_DIRS
        "."
        "../../common"
        "../../co2_sensor_common"
        "../../../ARM_CLION/common_lib/shell"
        "../../../ARM_CLION/common_lib"
)
