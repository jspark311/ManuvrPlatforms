cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
add_definitions(-DCONFIG_C3P_I2C -DCONFIG_C3P_CBOR)

# Add support for some helpful console tools.
add_definitions(-DCONFIG_C3P_CONSOLE_GPIO_TOOL -DCONFIG_C3P_CONSOLE_PFINFO_TOOL -DCONFIG_C3P_CONSOLE_REBOOT_TOOL)

#-DCONFIG_C3P_STORAGE
project(WiFiCommUnit)
