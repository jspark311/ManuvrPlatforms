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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <sys/utsname.h>

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


#define PROGRAM_VERSION    "0.0.2"    // Program version.
#define U_INPUT_BUFF_SIZE      512    // The maximum size of user input.


/*******************************************************************************
* TODO: Pending mitosis into a header file....
*******************************************************************************/

class CryptoLogShunt : public CryptOpCallback {
  public:
    CryptoLogShunt() {};
    ~CryptoLogShunt() {};

    /* Mandatory overrides from the CryptOpCallback interface... */
    int8_t op_callahead(CryptOp* op) {
      return JOB_Q_CALLBACK_NOMINAL;
    };

    int8_t op_callback(CryptOp* op) {
      StringBuilder output;
      op->printOp(&output);
      c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, &output);
      return JOB_Q_CALLBACK_NOMINAL;
    };
};

CryptoLogShunt crypto_logger;


/*******************************************************************************
* Globals
*******************************************************************************/
using namespace std;

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

LinuxUART*  uart   = nullptr;
ManuvrLink* m_link = nullptr;

ParsingConsole console(U_INPUT_BUFF_SIZE);
LinuxStdIO console_adapter;


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
    uint depth = (uint) args->position_as_int(1);
    if (0 == depth) {
      depth = 10;
    }
    CryptOpRNG* rng_op = new CryptOpRNG(&crypto_logger);
    rng_op->setResBuffer(nullptr, depth);
    rng_op->allocResBuffer(true);
    rng_op->reapJob(true);
    text_return->concatf("queue_job() returned %d\n", platformObj()->crypto->queue_job(rng_op));
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


int callback_gui_tools(StringBuilder* text_return, StringBuilder* args) {
  int ret = -1;
  // NOTE: The GUI is running in a separate thread. It should only be
  //   manipulated indirectly by an IPC mechanism, or by a suitable stand-ins.
  char* cmd = args->position_trimmed(0);
  if (0 == StringBuilder::strcasecmp(cmd, "resize")) {
    // NOTE: This is against GUI best-practices (according to Xorg). But it
    //   might be useful later.
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "gravepact")) {
    // If this is enabled, and the user closes the GUI, the main program will
    //   also terminate in an orderly manner.
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "detatched-modals")) {
    // If enabled, this setting causes modals to be spawned off as distinct
    //   child windows. If disabled, you will get an overlay instead.
  }
  return ret;
}


int callback_link_tools(StringBuilder* text_return, StringBuilder* args) {
  int ret = -1;
  char* cmd = args->position_trimmed(0);
  // We interdict if the command is something specific to this application.
  if (0 == StringBuilder::strcasecmp(cmd, "desc")) {
    // Send a description request message.
    KeyValuePair a((uint32_t) millis(), "time_ms");
    a.append((uint32_t) randomUInt32(), "rand");
    int8_t ret_local = m_link->send(&a, true);
    text_return->concatf("Description request send() returns ID %u\n", ret_local);
    ret = 0;
  }
  else ret = m_link->console_handler(text_return, args);

  return ret;
}


/*******************************************************************************
* This is a thread to run the GUI.
*******************************************************************************/
static void* gui_thread_handler(void*) {
  c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "Started GUI thread.");
  Display* dpy = XOpenDisplay(nullptr);
  bool keep_polling = (nullptr != dpy);
  if (keep_polling) {
    int screen_num = DefaultScreen(dpy);
    //Colormap color_map = XDefaultColormap(dpy, screen_num);
    Window win = XCreateSimpleWindow(
      dpy, RootWindow(dpy, screen_num),
      10,
      10,
      660,
      200,
      1,
      0x9932CC,  // TODO: Use colormap.
      0x000000   // TODO: Use colormap.
    );

    GC gc = DefaultGC(dpy, screen_num);
    XSetForeground(dpy, gc, 0xAAAAAA);
    XSelectInput(dpy, win, ExposureMask | KeyPressMask);
    XMapWindow(dpy, win);
    XStoreName(dpy, win, "Right Hand of Manuvr");

    Atom WM_DELETE_WINDOW = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &WM_DELETE_WINDOW, 1);
    bool uname_ok = false;
    struct utsname sname;
    int ret = uname(&sname);
    if (ret != -1) {
      uname_ok = true;
    }
    XEvent e;
    while (keep_polling) {
      if (0 < XPending(dpy)) {
        XNextEvent(dpy, &e);
        if (e.type == Expose) {
          XWindowAttributes wa;
          XGetWindowAttributes(dpy, win, &wa);
          int width = wa.width;
          int height = wa.height;
          char buf[256] = {0, };
          int y_offset = 10;
          const char* HEADER_STR_0 = "Right Hand of Manuvr";
          const char* HEADER_STR_1 = "Build date " __DATE__ " " __TIME__;
          XDrawString(dpy, win, gc, 4, y_offset, HEADER_STR_0, strlen(HEADER_STR_0));
          y_offset += 14;
          XDrawString(dpy, win, gc, 16, y_offset, HEADER_STR_1, strlen(HEADER_STR_1));
          y_offset += 14;

          if (uname_ok) {
            sprintf(buf, "%s %s (%s)", sname.sysname, sname.release, sname.machine);
            XDrawString(dpy, win, gc, 16, y_offset, buf, strlen(buf));
            y_offset += 14;
            sprintf(buf, "%s", sname.version);
            XDrawString(dpy, win, gc, 16, y_offset, buf, strlen(buf));
            y_offset += 14;
          }

          sprintf(buf, "Window size: %dx%d", width, height);
          XDrawString(dpy, win, gc, 4, y_offset, buf, strlen(buf));
          y_offset += 20;
        }

        if (e.type == KeyPress) {
          char buf[128] = {0, };
          KeySym keysym;
          int len = XLookupString(&e.xkey, buf, sizeof buf, &keysym, NULL);
          if (keysym == XK_Escape) {
            keep_polling = false;
          }
        }

        if ((e.type == ClientMessage) && (static_cast<unsigned int>(e.xclient.data.l[0]) == WM_DELETE_WINDOW)) {
          keep_polling = false;
        }
      }

      // If either this thread, or the main thread decided we should terminate,
      //   don't run another polling loop.
      keep_polling &= continue_running;
    }
    // We are about to end the thread.
    // Clean up the resources we allocated.
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
  }
  else {
    c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "Cannot open display.");
  }

  c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "Exiting GUI thread...");
  gui_thread_id = 0;
  return nullptr;
}



/*******************************************************************************
* The main function.                                                           *
*******************************************************************************/
int main(int argc, const char *argv[]) {
  program_name = argv[0];   // Our name.
  StringBuilder output;

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
      else if ((strlen(argv[i]) > 3) && (argv[i][0] == '-') && (argv[i][1] == '-')) {
        i++;
      }
      else {
        i++;
      }
    }
    else {
      printf("Unhandled argument: %s\n", argv[i]);
      //platform.firmware_shutdown(0);     // Clean up the platform.
    }
  }

  console.setTXTerminator(LineTerm::LF);
  console.setRXTerminator(LineTerm::LF);
  console.localEcho(false);

  // Mutually connect the console class to STDIO.
  console_adapter.readCallback(&console);
  console.setOutputTarget(&console_adapter);

  // We want to have a nice prompt string...
  StringBuilder prompt_string;
  prompt_string.concatf("%c[36m%s> %c[39m", 0x1B, argv[0], 0x1B);
  console.setPromptString((const char*) prompt_string.string());
  console.emitPrompt(true);
  console.hasColor(true);

  console.defineCommand("console",     '\0', ParsingConsole::tcodes_str_3, "Console conf.", "[echo|prompt|force|rxterm|txterm]", 0, callback_console_tools);
  if (0 == gui_thread_id) {
    console.defineCommand("gui",         'G', ParsingConsole::tcodes_str_4, "GUi tools.", "[echo|prompt|force|rxterm|txterm]", 0, callback_gui_tools);
  }
  console.defineCommand("crypto",     'C',  ParsingConsole::tcodes_str_4, "Cryptographic tools.", "", 0, callback_crypt_tools);
  console.defineCommand("link",        'l', ParsingConsole::tcodes_str_4, "Linked device tools.", "", 0, callback_link_tools);
  console.defineCommand("uart",        'u', ParsingConsole::tcodes_str_4, "UART tools.", "", 0, callback_uart_tools);
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
