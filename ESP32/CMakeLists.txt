#
# ESP-IDF component
#
message(STATUS "==============================================")
message(STATUS "ManuvrPlatform")
message(STATUS "==============================================")

file(GLOB_RECURSE SOURCE_FILES
  src/*.c
  src/*.cpp
)

# These are components that are known to work on ESP32.
idf_component_register(
  SRCS ${SOURCE_FILES}
  INCLUDE_DIRS "."
  REQUIRES CppPotpourri nvs_flash esp_adc_cal
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-ignored-qualifiers -Wno-implicit-fallthrough)
