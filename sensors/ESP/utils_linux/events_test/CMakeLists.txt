cmake_minimum_required(VERSION 4.2)
project(events_test C)

set(CMAKE_C_STANDARD 23)

add_executable(events_test main.c ../../../my/Programs/ESP_CLION/ieee802154/events.c)

include_directories(../../../my/Programs/ESP_CLION/ieee802154)
