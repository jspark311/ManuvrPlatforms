#include "AbstractPlatform.h"

/*******************************************************************************
*   _______                                   __   ____        __
*  /_  __(_)___ ___  ___     ____ _____  ____/ /  / __ \____ _/ /____
*   / / / / __ `__ \/ _ \   / __ `/ __ \/ __  /  / / / / __ `/ __/ _ \
*  / / / / / / / / /  __/  / /_/ / / / / /_/ /  / /_/ / /_/ / /_/  __/
* /_/ /_/_/ /_/ /_/\___/   \__,_/_/ /_/\__,_/  /_____/\__,_/\__/\___/
*******************************************************************************/

#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/string.hpp>

void currentDateTime(StringBuilder* target) {
  if (nullptr != target) {
    const godot::String DATE = godot::Time::get_singleton()->get_date_string_from_system(false);
    const godot::Dictionary TDICT = godot::Time::get_singleton()->get_time_dict_from_system(false);

    const int HOUR = (int) TDICT["hour"];
    const int MIN  = (int) TDICT["minute"];
    const int SEC  = (int) TDICT["second"];

    char buf[16];
    snprintf(buf, sizeof(buf), ".%02d:%02d:%02d", HOUR, MIN, SEC);

    target->concat(DATE.utf8().get_data());
    target->concat(buf);
  }
}


long unsigned millis() {
  return (long unsigned) godot::Time::get_singleton()->get_ticks_msec();
}

long unsigned micros() {
  return (long unsigned) godot::Time::get_singleton()->get_ticks_usec();
}

// We want monotonic time.
void sleep_ms(uint32_t ms) {  godot::OS::get_singleton()->delay_msec((int) ms);  }
void sleep_us(uint32_t us) {  godot::OS::get_singleton()->delay_usec((int) us);  }


/*******************************************************************************
*     __                      _
*    / /   ____  ____ _____ _(_)___  ____ _
*   / /   / __ \/ __ `/ __ `/ / __ \/ __ `/
*  / /___/ /_/ / /_/ / /_/ / / / / / /_/ /
* /_____/\____/\__, /\__, /_/_/ /_/\__, /
*             /____//____/        /____/
*******************************************************************************/
#include <godot_cpp/variant/utility_functions.hpp>

// Direct our logs into Godot's logging system.
void c3p_log(uint8_t severity, const char* tag, StringBuilder* msg) {
  const char* TAG_STR = (nullptr != tag) ? tag : "c3p";
  const char* MSG_STR = ((nullptr != msg) && (nullptr != msg->string())) ? (char*) msg->string() : "";

  godot::String line = godot::String("[") + TAG_STR + "] " + MSG_STR;

  switch (severity) {
    case 0:   // example: emergency
    case 1:
    case 2:
    case 3:
      godot::UtilityFunctions::printerr(line);
      break;
    case 4:
      godot::UtilityFunctions::push_warning(line);
      break;
    case 5:
    case 6:
      godot::UtilityFunctions::print(line);
      break;
    case 7:
    default:
      godot::UtilityFunctions::print_verbose(line);
      break;
  }
}



/*******************************************************************************
*     ______      __
*    / ____/___  / /__________  ____  __  __
*   / __/ / __ \/ __/ ___/ __ \/ __ \/ / / /
*  / /___/ / / / /_/ /  / /_/ / /_/ / /_/ /
* /_____/_/ /_/\__/_/   \____/ .___/\__, /
*                           /_/    /____/
*******************************************************************************/

#include <godot_cpp/classes/crypto.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

int8_t random_fill(uint8_t* buf, size_t len) {
  int8_t ret = -1;

  if ((nullptr != buf) && (0 < len)) {
    godot::Ref<godot::Crypto> crypto;
    crypto.instantiate();
    if (crypto.is_valid()) {
      const godot::PackedByteArray bytes = crypto->generate_random_bytes((int) len);
      if ((size_t) bytes.size() == len) {
        for (size_t i = 0; i < len; i++) {
          *(buf + i) = bytes[(int) i];
        }
        ret = 0;
      }
    }
  }

  return ret;
}


uint32_t randomUInt32() {
  uint32_t tmp;
  random_fill((uint8_t*) &tmp, 4);
  return tmp;
}
