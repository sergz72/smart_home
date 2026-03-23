cmake_minimum_required(VERSION 3.31)
project(device_emulator C)

set(CMAKE_C_STANDARD 23)

add_executable(device_emulator
        main.c
        ../../../my/Programs/ARM_CLION/common_lib/crypto/aes128.c
        ../../../my/Programs/ESP_CLION/common/crypto_common.c
)

include_directories(../../../my/Programs/ESP_CLION/common ../../../my/Programs/ARM_CLION/common_lib/crypto)
