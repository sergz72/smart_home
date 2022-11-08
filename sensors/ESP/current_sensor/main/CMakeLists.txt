idf_component_register(SRCS "../../common/main.c" "../../common/gatt_svr.c" "../../common/wifi.c" "../../common/ble.c"
        "../../common/nvs.c" "env.c" "../../common/led.c" "crypto.c" "tcp_client.c" "../../common/ntp.c" "queue.c"
        INCLUDE_DIRS "." "../../common"
)
