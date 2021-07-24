#
# ESP-IDF component
#
message(STATUS "==============================================")
message(STATUS "ManuvrPlatform")
message(STATUS "==============================================")

# These are components that are known to work on ESP32.
idf_component_register(
  SRC_DIRS "src"
  INCLUDE_DIRS "."
  REQUIRES CppPotpourri nvs_flash
)
