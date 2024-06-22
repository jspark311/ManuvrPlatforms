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

#define U_INPUT_BUFF_SIZE      512    // The maximum size of user input.

extern void* gui_thread_handler(void*);
int callback_gui_tools(StringBuilder*, StringBuilder*);

using namespace std;

/*******************************************************************************
* TODO: Pending mitosis into a header file....
*******************************************************************************/
IdentityUUID ident_uuid("BIN_ID", (char*) "29c6e2b9-9e68-4e52-9af0-03e9ca10e217");


/*******************************************************************************
* The program has a set of configurations that it defines and loads at runtime.
* This defines everything required to handle that conf fluidly and safely.
*
* TODO: Pending mitosis into a header file....
*******************************************************************************/
// Then, we bind those enum values each to a type code, and to a semantic string
//   suitable for storage or transmission to a counterparty.
const EnumDef<RHoMConfKey> CONF_KEY_LIST[] = {
  { RHoMConfKey::SHOW_PANE_MLINK,        "SHOW_PANE_MLINK",        0, (uint8_t) TCode::BOOLEAN    },
  { RHoMConfKey::MLINK_XPORT_PATH,       "MLINK_XPORT_PATH",       0, (uint8_t) TCode::STR        },
  { RHoMConfKey::MLINK_TIMEOUT_PERIOD,   "MLINK_TIMEOUT_PERIOD",   0, (uint8_t) TCode::UINT32     },
  { RHoMConfKey::MLINK_KA_PERIOD,        "MLINK_KA_PERIOD",        0, (uint8_t) TCode::UINT32     },
  { RHoMConfKey::MLINK_MTU,              "MLINK_MTU",              0, (uint8_t) TCode::UINT16     },
  { RHoMConfKey::MLINK_MAX_QUEUED_MSGS,  "MLINK_MAX_QUEUED_MSGS",  0, (uint8_t) TCode::UINT8      },
  { RHoMConfKey::MLINK_MAX_PARSE_FAILS,  "MLINK_MAX_PARSE_FAILS",  0, (uint8_t) TCode::UINT8      },
  { RHoMConfKey::MLINK_MAX_ACK_FAILS,    "MLINK_MAX_ACK_FAILS",    0, (uint8_t) TCode::UINT8      },
  { RHoMConfKey::MLINK_DEFAULT_ENCODING, "MLINK_DEFAULT_ENCODING", 0, (uint8_t) TCode::UINT8      },
  { RHoMConfKey::MLINK_REQUIRE_AUTH,     "MLINK_REQUIRE_AUTH",     0, (uint8_t) TCode::BOOLEAN    },
  { RHoMConfKey::MLINK_FARSIDE_LOGGING,  "MLINK_FARSIDE_LOGGING",  0, (uint8_t) TCode::BOOLEAN    },
  { RHoMConfKey::UART_BITRATE,           "UART_BITRATE",           0, (uint8_t) TCode::UINT32     },
  { RHoMConfKey::UART_START_BITS,        "UART_START_BITS",        0, (uint8_t) TCode::UINT8      },
  { RHoMConfKey::UART_BIT_PER_WORD,      "UART_BIT_PER_WORD",      0, (uint8_t) TCode::UINT8      },
  { RHoMConfKey::UART_STOP_BITS,         "UART_STOP_BITS",         0, (uint8_t) TCode::UINT8      },
  { RHoMConfKey::UART_PARITY,            "UART_PARITY",            0, (uint8_t) TCode::UINT8      },
  { RHoMConfKey::UART_FLOW_CONTROL,      "UART_FLOW_CONTROL",      0, (uint8_t) TCode::UINT8      },
  { RHoMConfKey::INVALID,                "INVALID", (ENUM_FLAG_MASK_INVALID_CATCHALL), 0}
};

// The top-level enum wrapper binds the above definitions into a tidy wad
//   of contained concerns.
const EnumDefList<RHoMConfKey> CONF_LIST(
  CONF_KEY_LIST, (sizeof(CONF_KEY_LIST) / sizeof(CONF_KEY_LIST[0])),
  "RHoMConfKey"  // Doesn't _need_ to be the enum name...
);

// After all that definition, we can finally create the conf object.
ConfRecordValidation<RHoMConfKey> rhom_conf(0, &CONF_LIST);


/*******************************************************************************
* Globals
*******************************************************************************/
const char*   program_name;
bool          continue_running  = true;

uint32_t ping_req_time = 0;
uint32_t ping_nonce    = 0;
MainGuiWindow* c3p_root_window   = nullptr;

M2MLinkOpts link_opts(
  100,   // ACK timeout is 100ms.
  2000,  // Send a KA every 2s.
  2048,  // MTU for this link is 2 kibi.
  TCode::CBOR,   // Payloads should be CBOR encoded.
  // This side of the link will send a KA while IDLE, and
  //   allows remote log write.
  (M2MLINK_FLAG_SEND_KA | M2MLINK_FLAG_ALLOW_LOG_WRITE)
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
M2MLink*    m_link = nullptr;

/* But some transports (socket servers) have 1:x relationships to sessions. */
LinuxSockListener socket_listener((char*) "/tmp/c3p-test.sock");

ParsingConsole console(U_INPUT_BUFF_SIZE);
LinuxStdIO console_adapter;
LinuxSockPipe socket_adapter;
LinkedList<LinkSockPair*> active_links;

int8_t new_socket_connection_callback(LinuxSockListener* svr, LinuxSockPipe* pipe) {
  int8_t ret = 0;   // We should reject by default.
  c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "New socket connection.");
  M2MLink* new_link = new M2MLink(&link_opts);
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

void link_callback_state(M2MLink* cb_link) {
  c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "Link (0x%x) entered state %s.", cb_link->linkTag(), M2MLink::sessionStateStr(cb_link->currentState()));
}


void link_callback_message(uint32_t session_tag, M2MMsg* msg) {
  StringBuilder log;
  KeyValuePair* kvps_rxd = nullptr;
  bool dump_msg_debug = true;
  log.concatf("link_callback_message(Tag = %u, ID = 0x%08x):\n", session_tag, msg->uniqueId());
  msg->getPayload(&kvps_rxd);
  if (kvps_rxd) {
    char* fxn_name = nullptr;
    if (0 == kvps_rxd->valueWithKey("fxn", &fxn_name)) {
      if (0 == strcmp("PING", fxn_name)) {
        //dump_msg_debug = false;
        // Counterparty may have replied to our ping. If not, reply logic will
        //   handle the response.
        if (ping_nonce) {
          if (ping_nonce == msg->uniqueId()) {
            log.concatf("\tPing returned in %ums.\n", micros_since(ping_req_time));
            ping_req_time = 0;
            ping_nonce    = 0;
          }
        }
      }
      else if (0 == strcmp("IMG_CAST", fxn_name)) {
        // Counterparty is sending us an image.
        dump_msg_debug = false;
      }
      else {
        log.concatf("\tUnhandled fxn: \n", fxn_name);
      }
    }
    else {
      log.concat("\tRX'd message with no specified fxn.\n");
    }
  }
  else {
    if (msg->isReply()) {
      log.concatf("\tRX'd ACK for msg %u.\n", msg->uniqueId());
      dump_msg_debug = false;
    }
    else {
      log.concat("\tRX'd message with no payload.\n");
    }
  }

  if (dump_msg_debug) {
    log.concat('\n');
    msg->printDebug(&log);
  }

  if (msg->expectsReply()) {
    int8_t ack_ret = msg->ack();
    log.concatf("\tlink_callback_message ACK'ing %u returns %d.\n", msg->uniqueId(), ack_ret);
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
          uart->readCallback(m_link);      // Attach the UART to M2MLink...
          m_link->setEfferant(uart);   // ...and M2MLink to UART.
        }
        else print_alloc_fail = true;
      }
      else text_return->concat("Usage:\t uart new [path-to-tty].\n");
    }
    else text_return->concat("Program currently only allows one UART at a time.\n");
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "free")) {
    if (nullptr != uart) {
      m_link->setEfferant(nullptr);   // Remove refs held elsewhere.
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
  int ret = -1;
  char* cmd = args->position_trimmed(0);
  // We interdict if the command is something specific to this application.
  if (0 == StringBuilder::strcasecmp(cmd, "ping")) {
    // Send a description request message.
    ping_req_time = (uint32_t) millis();
    ping_nonce = randomUInt32();
    KeyValuePair* a = new KeyValuePair("PING",  "fxn");
    a->append(ping_req_time, "time_ms");
    a->append(ping_nonce,    "rand");
    int8_t ret_local = m_link->send(a, true);
    text_return->concatf("Ping send() returns ID %u\n", ret_local);
    ret = 0;
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "screenshot")) {
    // TODO: I am using this space to prototype ImageCaster/Catcher.
    KeyValuePair a("IMG_CAST", "fxn");
    a.append((uint32_t) millis(),       "time_ms");
    int8_t ret_local = m_link->send(&a, false);
    text_return->concatf("IMG_CAST() returns ID %u\n", ret_local);
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
    text_return->concatf("write(%s) returned %d\n", (char*) args->string(), socket_adapter.pushBuffer(args));
  }
  else {
    ret = socket_adapter.console_handler(text_return, args);
  }
  return ret;
}


C3PScheduler* scheduler = nullptr;

/*******************************************************************************
* The main function.                                                           *
*******************************************************************************/
int main(int argc, const char *argv[]) {
  program_name = argv[0];   // Our name.
  StringBuilder output;

  platform.init();

  // Parse through all the command line arguments and flags...
  // Please note that the order matters. Put all the most-general matches at the bottom of the loop.
  for (int i = 1; i < argc; i++) {
    if ((strcasestr(argv[i], "--help")) || (strcasestr(argv[i], "-h"))) {
      printf("-u  --uart <path>   Instance a UART to talk to the hardware.\n");
      printf("-h  --help          Print this output and exit.\n");
      printf("-c  --conf          Use a non-default conf blob.\n");
      printf("    --conf-dump     Load the program configuration, dump it, and exit.\n");
      printf("\n\n");
      exit(0);
    }
    else if (strcasestr(argv[i], "--conf-dump")) {
      StringBuilder tmp_str;
      StringBuilder serialized;
      printf("Serializing conf returns %d.\n", rhom_conf.serialize(&serialized, TCode::CBOR));
      serialized.printDebug(&tmp_str);
      rhom_conf.printConfRecord(&tmp_str);
      printf("%s\n", (char*) tmp_str.string());
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
        char* uart_path = (char*) argv[i++];
        uart = new LinuxUART(uart_path);
        int8_t uart_init_ret = uart->init(&uart_opts);
        if (0 != uart_init_ret) {
          printf("Failed to initialize %s (returned %d).\n", uart_path, uart_init_ret);
          exit(1);
        }
        uart->readCallback(m_link);      // Attach the UART to M2MLink...
        m_link->setEfferant(uart);       // ...and M2MLink to UART.
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

  scheduler = C3PScheduler::getInstance();
  //if (nullptr == uart) {
  //  printf("You must supply a path to a UART.\n");
  //  exit(1);
  //}

  m_link = new M2MLink(&link_opts);
  m_link->setCallback(link_callback_state);
  m_link->setCallback(link_callback_message);
  m_link->localIdentity(&ident_uuid);
  //m_link->verbosity(6);


  // Instance an X11 window.
  // NOTE: The window destructor will block until the GUI thread is shut down.
  //   So for the sake of enforcing thread termination order, we empty-scope the
  //   existance of the window such that the process will naturally wait for all
  //   the complicated cleanup to happen before it flushes its output buffer,
  //   and slams the door on the process.
  {
    MainGuiWindow c3p_root_window(0, 0, 1280, 1024, argv[0]);
    if (0 == c3p_root_window.createWindow()) {
      // The window thread is running.
      StringBuilder output(program_name);
      output.concatf(" v%s initialized\n\n", PROGRAM_VERSION);
      c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, &output);
      do {   // The main loop. Run until told to stop.
        if (nullptr != m_link) {
          m_link->poll(&output);
          if (!output.isEmpty()) {
            console.printToLog(&output);
          }
        }
        console_adapter.poll();
        if (nullptr != uart) {
          uart->poll();
        }

        scheduler->serviceSchedules();
      } while (continue_running);   // GUI thread handles the heavy-lifting.
    }
    else {
      c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "Failed to create the root GUI window (not great, not terrible).");
    }
  }

  // Clean up any allocated stuff. It should already be hung up.
  if (nullptr != m_link) {
    m_link->hangup();
    delete m_link;
    m_link = nullptr;
  }
  if (nullptr != uart) {
    delete uart;
    uart = nullptr;
  }
  console_adapter.poll();   // Final chance for output to make it to the user.
  platform.firmware_shutdown(0);     // Clean up the platform.
  exit(0);  // Should never execute.
}
