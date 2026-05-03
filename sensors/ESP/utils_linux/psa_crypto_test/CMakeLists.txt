cmake_minimum_required(VERSION 4.2)
project(psa_crypto_test C)

set(CMAKE_C_STANDARD 23)

add_executable(psa_crypto_test main.c ../../../my/Programs/ESP_CLION/ieee802154/psa_crypto.c)

include_directories(../../../my/Programs/ESP_CLION/ieee802154 /usr/local/include/tf-psa-crypto)

target_link_libraries(psa_crypto_test /usr/local/lib/libtfpsacrypto.a)