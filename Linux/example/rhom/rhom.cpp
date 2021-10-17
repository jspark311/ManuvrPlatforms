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
  0      // No flags.
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
* Link callback
*******************************************************************************/

void link_callback(uint32_t tag, ManuvrMsg* msg) {
  StringBuilder log;
  KeyValuePair* kvps_rxd = nullptr;
  log.concatf("link_callback(0x%x): \n", tag, msg->uniqueId());
  msg->printDebug(&log);
  msg->getPayload(&kvps_rxd);
  if (msg->expectsReply()) {
    log.concatf("\nlink_callback ACK's %d.\n", msg->ack());
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


int callback_link_tools(StringBuilder* text_return, StringBuilder* args) {
  m_link->printDebug(text_return);
  return 0;
}


int callback_program_quit(StringBuilder* text_return, StringBuilder* args) {
  continue_running = 0;
  text_return->concat("Stopping...\n");
  console.emitPrompt(false);  // Avoid a trailing prompt.
  return 0;
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
        m_link->setCallback(link_callback);
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
  }
  if (nullptr != uart) {
    delete uart;
  }
  console_adapter.poll();

  platform.firmware_shutdown(0);     // Clean up the platform.
}
