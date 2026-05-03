idf_component_register(
        SRCS "main.cpp"
             "../../../../ieee802154/common_functions.c"
             "../../../../ieee802154/psa_crypto.c"
             "../../../../common/led.c"
             "../../../../common/wifi.c"
        INCLUDE_DIRS
             "."
             "../../../../common"
             "../../../../ieee802154"
)