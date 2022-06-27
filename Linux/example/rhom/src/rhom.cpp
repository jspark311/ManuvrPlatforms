/*
* Author:    J. Ian Lindsay
*
*/

#include <cstdio>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <math.h>

#include <fstream>
#include <iostream>

#include "RHoM.h"

#include "CppPotpourri.h"
#include "AbstractPlatform.h"
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
#include <CryptoBurrito/CryptoBurrito.h>
#include <Linux.h>



#define U_INPUT_BUFF_SIZE      512    // The maximum size of user input.

extern void* gui_thread_handler(void*);
extern int callback_gui_tools(StringBuilder*, StringBuilder*);
extern Image _main_img;


/*******************************************************************************
* TODO: Pending mitosis into a header file....
*******************************************************************************/

IdentityUUID ident_uuid("BIN_ID", "29c6e2b9-9e68-4e52-9af0-03e9ca10e217");


/*******************************************************************************
* Globals
*******************************************************************************/
using namespace std;

CryptoLogShunt crypto_logger;
const char*   program_name;
bool          continue_running  = true;
unsigned long gui_thread_id     = 0;


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

/* Transports such as UARTs are generally 1:1 with sessions. */
LinuxUART*  uart   = nullptr;
ManuvrLink* m_link = nullptr;

/* But some transports (socket servers) have 1:x relationships to sessions. */
LinuxSockListener socket_listener("/tmp/c3p-test.sock");

SensorFilter<uint32_t> _filter(128, FilteringStrategy::RAW);

ParsingConsole console(U_INPUT_BUFF_SIZE);
LinuxStdIO console_adapter;
LinuxSockPipe socket_adapter;


LinkedList<LinkSockPair*> active_links;



int8_t new_socket_connection_callback(LinuxSockListener* svr, LinuxSockPipe* pipe) {
  int8_t ret = 0;   // We should reject by default.
  c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "New socket connection.");
  ManuvrLink* new_link = new ManuvrLink(&link_opts);
  if (nullptr != new_link) {
    active_links.insert(new LinkSockPair(pipe, new_link));
    m_link = new_link;
    ret = 1;   // Accept connection.
  }
  return ret;
}




/*******************************************************************************
* Link callbacks
*******************************************************************************/

void link_callback_state(ManuvrLink* cb_link) {
  c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "Link (0x%x) entered state %s.", cb_link->linkTag(), ManuvrLink::sessionStateStr(cb_link->getState()));
}


void link_callback_message(uint32_t session_tag, ManuvrMsg* msg) {
  StringBuilder log;
  KeyValuePair* kvps_rxd = nullptr;
  log.concatf("link_callback_message(0x%x, 0x08%x): \n", session_tag, msg->uniqueId());
  msg->printDebug(&log);
  msg->getPayload(&kvps_rxd);
  if (kvps_rxd) {
    kvps_rxd->printDebug(&log);
  }
  if (msg->expectsReply()) {
    int8_t ack_ret = msg->ack();
    log.concatf("link_callback_message ACK'ing %u returns %d.\n", msg->uniqueId(), ack_ret);
  }
  c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, &log);
}


/*******************************************************************************
* Console callbacks
*******************************************************************************/

int callback_help(StringBuilder* text_return, StringBuilder* args) {
  text_return->concatf("RHoM %s\n", PROGRAM_VERSION);
  return console.console_handler_help(text_return, args);
}

int callback_console_tools(StringBuilder* text_return, StringBuilder* args) {
  return console.console_handler_conf(text_return, args);
}


int callback_program_quit(StringBuilder* text_return, StringBuilder* args) {
  continue_running = false;
  text_return->concat("Stopping...\n");
  console.emitPrompt(false);  // Avoid a trailing prompt.
  return 0;
}

int callback_crypt_tools(StringBuilder* text_return, StringBuilder* args) {
  int ret = 0;
  char* cmd = args->position_trimmed(0);
  if (0 == StringBuilder::strcasecmp(cmd, "info")) {
    platformObj()->crypto->printDebug(text_return);
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "queue")) {
    platformObj()->crypto->printQueues(text_return);
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "poll")) {
    int8_t ret_local = platformObj()->crypto->poll();
    text_return->concatf("poll() returned %d\n", ret_local);
  }

  else if (0 == StringBuilder::strcasecmp(cmd, "rng")) {
    uint depth = (uint) args->position_as_int(1);
    if (0 == depth) {
      depth = 10;
    }
    CryptOpRNG* rng_op = new CryptOpRNG(&crypto_logger);
    uint8_t* buf = (uint8_t*) malloc(depth);
    if (buf) {
      memset(buf, 0, depth);
      rng_op->setResBuffer(buf, depth);
      rng_op->freeResBuffer(true);
      rng_op->reapJob(true);
      text_return->concatf("queue_job() returned %d\n", platformObj()->crypto->queue_job(rng_op));
    }
  }

  else if (0 == StringBuilder::strcasecmp(cmd, "rng2")) {
    uint depth = _main_img.bytesUsed();
    if (0 != depth) {
      CryptOpRNG* rng_op = new CryptOpRNG(nullptr);
      rng_op->setResBuffer(_main_img.buffer(), depth);
      rng_op->reapJob(true);
      text_return->concatf("queue_job() returned %d\n", platformObj()->crypto->queue_job(rng_op));
    }
  }
  return ret;
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
          text_return->concat("Attempting to set bitrate on UART...\n");
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


uint32_t ping_req_time = 0;
uint32_t ping_nonce    = 0;

int callback_link_tools(StringBuilder* text_return, StringBuilder* args) {
  int ret = -1;
  char* cmd = args->position_trimmed(0);
  // We interdict if the command is something specific to this application.
  if (0 == StringBuilder::strcasecmp(cmd, "desc")) {
    // Send a self-description message, with a request for same.
    KeyValuePair a("WHO", "fxn");
    a.append(&ident_uuid,      "ident");
    int8_t ret_local = m_link->send(&a, true);
    text_return->concatf("Description request send() returns ID %u\n", ret_local);
    ret = 0;
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "ping")) {
    // Send a description request message.
    ping_req_time = (uint32_t) millis();
    ping_nonce = randomUInt32();
    KeyValuePair a("PING",  "fxn");
    a.append(ping_req_time, "time_ms");
    a.append(ping_nonce,    "rand");
    int8_t ret_local = m_link->send(&a, true);
    text_return->concatf("Description request send() returns ID %u\n", ret_local);
    ret = 0;
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "screenshot")) {
    // TODO: I am using this space to prototype ImageCaster/Catcher.
    KeyValuePair a("IMG_CAST", "fxn");
    a.append((uint32_t) millis(),       "time_ms");
    int8_t ret_local = m_link->send(&a, false);
    text_return->concatf("Description request send() returns ID %u\n", ret_local);
    ret = 0;
  }
  else ret = m_link->console_handler(text_return, args);

  return ret;
}


int callback_socket_tools(StringBuilder* text_return, StringBuilder* args) {
  int ret = -1;
  char* cmd = args->position_trimmed(0);
  // We interdict if the command is something specific to this application.
  if (0 == StringBuilder::strcasecmp(cmd, "listener")) {
    ret = socket_listener.console_handler(text_return, args);
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "twrite")) {
    args->drop_position(0);
    text_return->concatf("write(%s) returned %d\n", (char*) args->string(), socket_adapter.provideBuffer(args));
  }
  else {
    ret = socket_adapter.console_handler(text_return, args);
  }
  return ret;
}



/*******************************************************************************
* The main function.                                                           *
*******************************************************************************/
int main(int argc, const char *argv[]) {
  program_name = argv[0];   // Our name.
  StringBuilder output;

  platform.init();
  _filter.init();

  m_link = new ManuvrLink(&link_opts);
  m_link->setCallback(link_callback_state);
  m_link->setCallback(link_callback_message);

  // Parse through all the command line arguments and flags...
  // Please note that the order matters. Put all the most-general matches at the bottom of the loop.
  for (int i = 1; i < argc; i++) {
    if ((strcasestr(argv[i], "--help")) || (strcasestr(argv[i], "-h"))) {
      printf("-u  --uart <path>   Instance a UART to talk to the hardware.\n");
      printf("-v  --version       Print the version and exit.\n");
      printf("    --gui           Run the GUI.\n");
      printf("-h  --help          Print this output and exit.\n");
      printf("\n\n");
      exit(0);
    }
    else if ((strcasestr(argv[i], "--version")) || (strcasestr(argv[i], "-v") == argv[i])) {
      printf("RHoM v%s\n\n", PROGRAM_VERSION);
      exit(0);
    }
    else if (strcasestr(argv[i], "--gui")) {
      // Instance an X11 window.
      if (0 == gui_thread_id) {
        platform.createThread(&gui_thread_id, nullptr, gui_thread_handler, nullptr, nullptr);
      }
    }
    else if (argc - i >= 2) {    // Compound arguments go in this case block...
      if ((strcasestr(argv[i], "--uart")) || (strcasestr(argv[i], "-u"))) {
        if (argc - i < 2) {  // Mis-use of flag...
          printf("Using --uart means you must supply a path to it.\n");
          exit(1);
        }
        i++;
        // Instance a UART.
        uart = new LinuxUART((char*) argv[i++]);
        uart->init(&uart_opts);
        uart->readCallback(m_link);      // Attach the UART to ManuvrLink...
        m_link->setOutputTarget(uart);   // ...and ManuvrLink to UART.
      }
      else if ((strcasestr(argv[i], "--listen")) || (strcasestr(argv[i], "-L"))) {
        if (argc - i < 2) {  // Mis-use of flag...
          printf("Using --listen means you must supply a path.\n");
          exit(1);
        }
        i++;
        socket_listener.newConnectionCallback(new_socket_connection_callback);
        socket_listener.listen((char*) argv[i++]);
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
    }
  }

  console.setTXTerminator(LineTerm::LF);
  console.setRXTerminator(LineTerm::LF);
  StringBuilder prompt_string;   // We want to have a nice prompt string...
  if (0 == gui_thread_id) {
    // The GUI thread handles the console, if it was enabled. If there is no
    //   GUI, mutually connect the console class to STDIO.
    console.localEcho(false);
    console_adapter.readCallback(&console);
    console.setOutputTarget(&console_adapter);
    console.hasColor(true);
    prompt_string.concatf("%c[36m%s> %c[39m", 0x1B, argv[0], 0x1B);
  }
  else {
    prompt_string.concatf("%s> ", argv[0]);
  }
  console.setPromptString((const char*) prompt_string.string());
  console.emitPrompt(true);


  console.defineCommand("console",     '\0', ParsingConsole::tcodes_str_3, "Console conf.", "[echo|prompt|force|rxterm|txterm]", 0, callback_console_tools);
  if (0 != gui_thread_id) {
    console.defineCommand("gui",         'G', ParsingConsole::tcodes_str_4, "GUi tools.", "[echo|prompt|force|rxterm|txterm]", 0, callback_gui_tools);
  }
  console.defineCommand("crypto",     'C',  ParsingConsole::tcodes_str_4, "Cryptographic tools.", "", 0, callback_crypt_tools);
  console.defineCommand("link",        'l', ParsingConsole::tcodes_str_4, "Linked device tools.", "", 0, callback_link_tools);
  console.defineCommand("uart",        'u', ParsingConsole::tcodes_str_4, "UART tools.", "", 0, callback_uart_tools);
  console.defineCommand("socket",      'S', ParsingConsole::tcodes_str_4, "Socket tools.", "", 0, callback_socket_tools);
  console.defineCommand("quit",        'Q', ParsingConsole::tcodes_0, "Commit sudoku.", "", 0, callback_program_quit);
  console.defineCommand("help",        '?', ParsingConsole::tcodes_str_1, "Prints help to console.", "[<specific command>]", 0, callback_help);
  platform.configureConsole(&console);

  console.init();
  output.concatf("%s initialized.\n", argv[0]);
  console.printToLog(&output);
  console.printPrompt();

  // The main loop. Run until told to stop.
  while (continue_running) {
    if (nullptr != m_link) {
      m_link->poll(&output);
      if (!output.isEmpty()) {
        console.printToLog(&output);
      }
    }
    console_adapter.poll();
  }
  console.emitPrompt(false);  // Avoid a trailing prompt.

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

  while (0 != gui_thread_id) {
    sleep_ms(10);
  }

  platform.firmware_shutdown(0);     // Clean up the platform.
  return 0;  // Should never execute.
}
