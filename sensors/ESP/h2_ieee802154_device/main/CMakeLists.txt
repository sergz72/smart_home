idf_component_register(
        SRCS "main.cpp"
             "../../ieee802154/common_functions.c"
             "../../ieee802154/psa_crypto.c"
             "../../ieee802154/hal.c"
             "../../ieee802154/scd4x_functions.c"
             "../../ieee802154/sht4x_functions.c"
             "../../common/led.c"
             "../../common/scd4x.c"
             "../../common/sht4x.c"
             "../../common/crc8.c"
        INCLUDE_DIRS
             "."
             "../../common"
             "../../ieee802154"
)
