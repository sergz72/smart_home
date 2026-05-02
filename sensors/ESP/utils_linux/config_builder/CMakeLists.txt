cmake_minimum_required(VERSION 4.2)
project(config_builder C)

set(CMAKE_C_STANDARD 23)

add_executable(config_builder main.c)

include_directories(../../../my/Programs/ESP_CLION/ieee802154)
