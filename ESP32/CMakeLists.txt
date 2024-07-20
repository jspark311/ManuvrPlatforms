#
# ESP-IDF component
#
message(STATUS "==============================================")
message(STATUS "ManuvrPlatform-ESP32")
message(STATUS "==============================================")

file(GLOB_RECURSE srcs
  src/*.c
  src/*.cpp
)

idf_component_register(
  SRCS "${srcs}"
  INCLUDE_DIRS "."
  PRIV_INCLUDE_DIRS "."
  REQUIRES CppPotpourri esp_event
  PRIV_REQUIRES nvs_flash driver esp_adc efuse esp_timer esp_wifi esp_driver_gpio esp_driver_ledc
)

target_compile_options(${COMPONENT_LIB} PUBLIC -Wno-ignored-qualifiers -Wno-implicit-fallthrough)
