#include <cstdio>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <math.h>

#include <fstream>
#include <iostream>

#include "CppPotpourri.h"
#include "StringBuilder.h"
#include "ParsingConsole.h"
#include "ElementPool.h"
#include "GPSWrapper.h"
#include "UARTAdapter.h"
#include "I2CAdapter.h"
#include "KeyValuePair.h"
#include "SensorFilter.h"
#include "Vector3.h"
#include "StopWatch.h"
#include "uuid.h"
#include "cbor-cpp/cbor.h"
#include "Image/Image.h"
#include "Identity/IdentityUUID.h"
#include "Identity/Identity.h"
#include "ManuvrLink/ManuvrLink.h"

#include <Linux.h>

#define FP_VERSION         "0.0.1"    // Program version.
#define U_INPUT_BUFF_SIZE     8192    // The maximum size of user input.

/*******************************************************************************
* Globals
*******************************************************************************/
int continue_running  = 1;
int parent_pid        = 0;            // The PID of the root process (always).
char* program_name;

/* Console junk... */
ParsingConsole console(U_INPUT_BUFF_SIZE);
LinuxStdIO console_adapter;


ManuvrLinkOpts link_opts(
  100,   // ACK timeout is 100ms.
  2000,  // Send a KA every 2s.
  2048,  // MTU for this link is 2 kibi.
  TCode::CBOR,   // Payloads should be CBOR encoded.
  // This side of the link will send a KA while IDLE, and
  //   allows remote log write.
  (MANUVRLINK_FLAG_SEND_KA | MANUVRLINK_FLAG_ALLOW_LOG_WRITE)
);

UARTOpts uart_opts {
  .bitrate       = 115200,
  .start_bits    = 0,
  .bit_per_word  = 8,
  .stop_bits     = UARTStopBit::STOP_1,
  .parity        = UARTParityBit::NONE,
  .flow_control  = UARTFlowControl::NONE,
  .xoff_char     = 0,
  .xon_char      = 0,
  .padding       = 0
};

LinuxUART*  uart   = nullptr;
ManuvrLink* m_link = nullptr;



/*******************************************************************************
* Link callbacks
*******************************************************************************/


void link_callback_state(ManuvrLink* cb_link) {
  StringBuilder log;
  log.concatf("Link (0x%x) entered state %s\n", cb_link->linkTag(), ManuvrLink::sessionStateStr(cb_link->getState()));
  printf("%s\n\n", (const char*) log.string());
}


void link_callback_message(uint32_t tag, ManuvrMsg* msg) {
  StringBuilder log;
  KeyValuePair* kvps_rxd = nullptr;
  log.concatf("link_callback_message(0x%x): \n", tag, msg->uniqueId());
  msg->printDebug(&log);
  msg->getPayload(&kvps_rxd);
  if (kvps_rxd) {
    //kvps_rxd->printDebug(&log);
  }
  if (msg->expectsReply()) {
    int8_t ack_ret = msg->ack();
    log.concatf("\nlink_callback_message ACK'ing %u returns %d.\n", msg->uniqueId(), ack_ret);
  }
  printf("%s\n\n", (const char*) log.string());
}


/*******************************************************************************
* Console callbacks
*******************************************************************************/

int callback_help(StringBuilder* text_return, StringBuilder* args) {
  text_return->concatf("%s %s\n", program_name, FP_VERSION);
  if (0 < args->count()) {
    console.printHelp(text_return, args->position_trimmed(0));
  }
  else {
    console.printHelp(text_return);
  }
  return 0;
}

int callback_print_history(StringBuilder* text_return, StringBuilder* args) {
  console.printHistory(text_return);
  return 0;
}


int callback_program_quit(StringBuilder* text_return, StringBuilder* args) {
  continue_running = 0;
  text_return->concat("Stopping...\n");
  console.emitPrompt(false);  // Avoid a trailing prompt.
  return 0;
}


int callback_uart_tools(StringBuilder* text_return, StringBuilder* args) {
  int ret = 0;
  char* cmd = args->position_trimmed(0);
  bool print_alloc_fail = false;
  if (0 == StringBuilder::strcasecmp(cmd, "info")) {
    if (nullptr != uart) {
      uart->printDebug(text_return);
    }
    else print_alloc_fail = true;
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "reset")) {
    if (nullptr != uart) {
      uart->reset();
      text_return->concat("UART was reset().\n");
    }
    else print_alloc_fail = true;
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "bitrate")) {
    if (nullptr != uart) {
      switch (args->count()) {
        case 2:
          uart->uartOpts()->bitrate = args->position_as_int(1);
          uart->reset();
          text_return->concatf("Attempting to set bitrate on UART...\n");
          break;
        default:
          text_return->concatf("UART bitrate is %u\n", uart->uartOpts()->bitrate);
          break;
      }
    }
    else print_alloc_fail = true;
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "new")) {
    if (nullptr == uart) {
      if (2 == args->count()) {
        // Instance a UART.
        uart = new LinuxUART(args->position_trimmed(1));
        if (nullptr != uart) {
          uart->init(&uart_opts);
          uart->readCallback(m_link);      // Attach the UART to ManuvrLink...
          m_link->setOutputTarget(uart);   // ...and ManuvrLink to UART.
        }
        else print_alloc_fail = true;
      }
      else text_return->concat("Usage:\t uart new [path-to-tty].\n");
    }
    else text_return->concat("Program currently only allows one UART at a time.\n");
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "free")) {
    if (nullptr != uart) {
      m_link->setOutputTarget(nullptr);   // Remove refs held elsewhere.
      delete uart;
      uart = nullptr;
    }
    else print_alloc_fail = true;
  }
  else text_return->concat("Usage:\t uart [info|reset|bitrate|new|free]\n");

  if (print_alloc_fail) {
    text_return->concat("UART unallocated.\n");
  }
  return ret;
}


int callback_link_tools(StringBuilder* text_return, StringBuilder* args) {
  int ret = 0;
  char* cmd = args->position_trimmed(0);
  if (0 == StringBuilder::strcasecmp(cmd, "info")) {
    m_link->printDebug(text_return);
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "reset")) {
    text_return->concatf("Link reset() returns %d\n", m_link->reset());
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "hangup")) {
    text_return->concatf("Link hangup() returns %d\n", m_link->hangup());
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "verbosity")) {
    switch (args->count()) {
      case 2:
        m_link->verbosity(0x07 & args->position_as_int(1));
      default:
        text_return->concatf("Link verbosity is %u\n", m_link->verbosity());
        break;
    }
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "log")) {
    //if (1 < args->count()) {
      StringBuilder tmp_log("This is a remote log test.\n");
      int8_t ret_local = m_link->writeRemoteLog(&tmp_log, false);
      text_return->concatf("Remote log write returns %d\n", ret_local);
    //}
    //else text_return->concat("Usage: link log <logText>\n");
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "desc")) {
    // Send a description request message.
    KeyValuePair a((uint32_t) millis(), "time_ms");
    a.append((uint32_t) randomUInt32(), "rand");
    int8_t ret_local = m_link->send(&a, true);
    text_return->concatf("Description request send() returns ID %u\n", ret_local);
  }
  else text_return->concat("Usage: [info|reset|hangup|verbosity|desc]\n");

  return ret;
}



/*******************************************************************************
* The main function.                                                           *
*******************************************************************************/
int main(int argc, char *argv[]) {
  StringBuilder output;
  parent_pid = getpid();    // We will need to know our root PID.
  program_name = argv[0];   // Our name.

  platform.init();

  m_link = new ManuvrLink(&link_opts);
  m_link->setCallback(link_callback_state);
  m_link->setCallback(link_callback_message);

  // Parse through all the command line arguments and flags...
  // Please note that the order matters. Put all the most-general matches at the bottom of the loop.
  for (int i = 1; i < argc; i++) {
    if ((strcasestr(argv[i], "--help")) || (strcasestr(argv[i], "-h"))) {
      printf("-u  --uart <path>   Instance a UART to talk to the hardware.\n");
      printf("-v  --version       Print the version and exit.\n");
      printf("-h  --help          Print this output and exit.\n");
      printf("\n\n");
      exit(0);
    }
    else if ((strcasestr(argv[i], "--version")) || (strcasestr(argv[i], "-v") == argv[i])) {
      printf("%s v%s\n\n", argv[0], FP_VERSION);
      exit(0);
    }
    else if (argc - i >= 2) {    // Compound arguments go in this case block...
      if ((strcasestr(argv[i], "--uart")) || (strcasestr(argv[i], "-u"))) {
        if (argc - i < 2) {  // Mis-use of flag...
          printf("Using --uart means you must supply a path to it.\n");
          exit(1);
        }
        i++;
        // Instance a UART.
        uart = new LinuxUART(argv[i++]);
        uart->init(&uart_opts);
        uart->readCallback(m_link);      // Attach the UART to ManuvrLink...
        m_link->setOutputTarget(uart);   // ...and ManuvrLink to UART.
      }
      else if ((strlen(argv[i]) > 3) && (argv[i][0] == '-') && (argv[i][1] == '-')) {
        i++;
      }
      else {
        i++;
      }
    }
    else {
      printf("Unhandled argument: %s\n", argv[i]);
      platform.firmware_shutdown(0);     // Clean up the platform.
    }
  }

  // Mutually connect the console class to STDIO.
  console_adapter.readCallback(&console);
  console.setOutputTarget(&console_adapter);

  // We want to have a nice prompt string...
  StringBuilder prompt_string;
  prompt_string.concatf("%c[36m%s> %c[39m", 0x1B, argv[0], 0x1B);
  console.setPromptString((const char*) prompt_string.string());

  console.defineCommand("help",        '?', ParsingConsole::tcodes_str_1, "Prints help to console.", "", 0, callback_help);
  console.defineCommand("history",     ParsingConsole::tcodes_0, "Print command history.", "", 0, callback_print_history);
  console.defineCommand("link",        'l', ParsingConsole::tcodes_str_4, "Linked device tools.", "", 0, callback_link_tools);
  console.defineCommand("uart",        'u', ParsingConsole::tcodes_str_4, "UART tools.", "", 0, callback_uart_tools);
  platform.configureConsole(&console);
  console.defineCommand("quit",        'Q', ParsingConsole::tcodes_0, "Commit sudoku.", "", 0, callback_program_quit);


  console.setTXTerminator(LineTerm::LF);
  console.setRXTerminator(LineTerm::LF);
  console.localEcho(false);
  console.emitPrompt(true);
  console.hasColor(true);
  console.init();

  output.concatf("%s initialized.\n", argv[0]);
  console.printToLog(&output);
  console.printPrompt();

  // The main loop. Run until told to stop.
  while (continue_running) {
    console_adapter.poll();
    if (nullptr != m_link) {
      m_link->poll(&output);
    }
    console.printToLog(&output);
  }
  console.emitPrompt(false);  // Avoid a trailing prompt.
  console.printToLog(&output);

  if (nullptr != m_link) {
    m_link->hangup();
    delete m_link;
    m_link = nullptr;
  }
  if (nullptr != uart) {
    delete uart;
    uart = nullptr;
  }
  console_adapter.poll();

  platform.firmware_shutdown(0);     // Clean up the platform.
}
