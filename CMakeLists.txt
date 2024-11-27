cmake_minimum_required(VERSION 3.20.0)
set(BOARD qemu_x86)
#set(EXTRA_ZEPHYR_MODULES modules/S2OPC)

find_package(Zephyr)
project(opcua_pubsub_app)

target_sources(app PRIVATE src/main.c)
target_link_libraries(app PRIVATE S2OPC)
