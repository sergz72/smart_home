idf_component_register(
        SRCS "main.c"
             "../../common/led.c"
             "../../../ARM_CLION/common_lib/getstring.c"
             "../../../ARM_CLION/common_lib/shell/shell.c"
             "../../../ARM_CLION/common_lib/fixed_queue.c"
        INCLUDE_DIRS
             "."
             "../../common"
             "../../../ARM_CLION/common_lib/shell"
             "../../../ARM_CLION/common_lib"
)
