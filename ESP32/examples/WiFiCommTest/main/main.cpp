#include <math.h>
#include "WiFiCommUnit.h"

#ifdef __cplusplus
extern "C" {
#endif
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "mqtt_client.h"
#ifdef __cplusplus
}
#endif


/*******************************************************************************
* Globals
*******************************************************************************/
static const char* TAG         = "main-cpp";
const char* console_prompt_str = "CommUnit # ";
IdentityUUID ident_uuid("CommUnit", (char*) "50eecd6e-d567-4100-a3c3-582671908e2c");

/* Profiling data */
StopWatch stopwatch_main_loop_time;

/* Cheeseball async support stuff. */
uint32_t boot_time         = 0;      // millis() at boot.
uint32_t config_time       = 0;      // millis() at end of setup().
uint32_t last_interaction  = 0;      // millis() when the user last interacted.

UARTOpts uart2_opts {
  .bitrate       = 9600,
  .start_bits    = 0,
  .bit_per_word  = 8,
  .stop_bits     = UARTStopBit::STOP_1,
  .parity        = UARTParityBit::NONE,
  .flow_control  = UARTFlowControl::NONE,
  .xoff_char     = 0,
  .xon_char      = 0,
  .padding       = 0
};


ParsingConsole console(128);
ESP32StdIO console_uart;
PlatformUART ext_uart(2, UART2_TX_PIN, UART2_RX_PIN, 255, 255, 256, 256);

ESP32Radio* esp_radio = nullptr;
MQTTClient  esp_mqtt;


/*******************************************************************************
* Console callbacks
*******************************************************************************/

/* Direct console shunt */
int callback_help(StringBuilder* txt_ret, StringBuilder* args) {
  return console.console_handler_help(txt_ret, args);
}

/* Direct console shunt */
int callback_console_tools(StringBuilder* txt_ret, StringBuilder* args) {
  return console.console_handler_conf(txt_ret, args);
}

int callback_wifi_tools(StringBuilder* txt_ret, StringBuilder* args) {
  return esp_radio->console_handler_esp_radio(txt_ret, args);
}

int callback_mqtt_tools(StringBuilder* txt_ret, StringBuilder* args) {
  return esp_mqtt.console_handler_mqtt_client(txt_ret, args);
}


#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
* Support functions
*******************************************************************************/


/*******************************************************************************
* Threads
*******************************************************************************/

/**
* This is the Manuvr thread that runs constatntly, and would be the main loop
*   of a single-threaded program.
*/
void c3p_task(void* pvParameter) {
  while (1) {
    bool should_sleep = true;

    if (0 < (int8_t) console_uart.poll()) { should_sleep = false; }
    if (0 < (int8_t) ext_uart.poll())     { should_sleep = false; }
    if (0 < (int8_t) esp_radio->poll())   { should_sleep = false; }
    if (0 < (int8_t) esp_mqtt.poll())     { should_sleep = false; }

    platform.yieldThread();
  }
}


/*******************************************************************************
* Setup function. This will be the entry-point for our code from ESP-IDF's
*   boilerplate. Since we don't trust the sdkconfig to have specified a stack
*   of appropriate depth, we do our setup, launch any threads we want, and the
*   let this thread die.
*******************************************************************************/
void app_main() {
  /*
  * The platform object is created on the stack, but takes no action upon
  *   construction. The first thing that should be done is to call the preinit
  *   function to setup the defaults of the platform.
  */
  platform_init();
  boot_time = millis();

  // The unit has a bi-color LED with a common anode.
  // Start with the LED off.
  pinMode(LED_R_PIN,  GPIOMode::ANALOG_OUT);
  pinMode(LED_G_PIN,  GPIOMode::ANALOG_OUT);
  analogWrite(LED_G_PIN, 1.0);
  analogWrite(LED_R_PIN, 1.0);

  esp_radio = ESP32Radio::getInstance();

  /* Start the console UART and attach it to the console. */
  console_uart.readCallback(&console);    // Attach the UART to console...
  console.setEfferant(&console_uart);     // ...and console to UART.
  console.setRXTerminator(LineTerm::LF);  // Best setting for "idf.py monitor"
  console.setPromptString(console_prompt_str);
  console.emitPrompt(true);
  console.localEcho(true);
  console.printHelpOnFail(true);

  platform.configureConsole(&console);
  console.defineCommand("console",  '\0', "Console conf.", "[echo|prompt|force|rxterm|txterm]", 0, callback_console_tools);
  console.defineCommand("help",     '?',  "Prints help to console.", "[<specific command>]", 0, callback_help);
  console.defineCommand("wifi",     'w',  "WiFi tools", "[con|discon|scan]", 0, callback_wifi_tools);
  console.defineCommand("mqtt",     'm',  "MQTT tools", "[con|discon|list]", 0, callback_mqtt_tools);

  StringBuilder ptc("CommUnit " TEST_PROG_VERSION "\t Build date " __DATE__ " " __TIME__ "\n");

  // Bind MQTT to Radio *before* init.
  esp_mqtt.setRadio(esp_radio);

  esp_radio->init();
  esp_mqtt.init();
  console.init();

  // Write our boot log to the UART.
  console.printToLog(&ptc);

  // Spawn worker thread for C3P's use.
  xTaskCreate(c3p_task, "_c3p", 32768, NULL, (tskIDLE_PRIORITY), NULL);

  // Note the time it took to do setup, and allow THIS thread to gracefully terminate.
  config_time = millis();
}

#ifdef __cplusplus
}
#endif
