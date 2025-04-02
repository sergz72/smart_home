idf_component_register(SRCS "../../common/main.c" "../../common/gatt_svr.c" "../../common/wifi.c" "../../common/ble.c"
        "../../common/nvs.c" "env.c" "../../common/led.c" "../../common/ntp.c" "../../common/net_client_common.c"
        "../../common/crypto_common.c"
        "crypto.c" "tcp_client.c" "queue.c"
        INCLUDE_DIRS "." "../../common"
)
