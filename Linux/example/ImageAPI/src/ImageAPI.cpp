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

#include "ImageAPI.h"

#define U_INPUT_BUFF_SIZE      512    // The maximum size of user input.

extern void* gui_thread_handler(void*);
int callback_gui_tools(StringBuilder*, StringBuilder*);

using namespace std;


/*******************************************************************************
* Globals
*******************************************************************************/
const char*   program_name;
bool          continue_running  = true;

MainGuiWindow* c3p_root_window   = nullptr;

ParsingConsole console(U_INPUT_BUFF_SIZE);
LinuxStdIO console_adapter;


/*******************************************************************************
* Console callbacks
*******************************************************************************/

int callback_help(StringBuilder* text_return, StringBuilder* args) {
  text_return->concatf("%s %s\n", program_name, PROGRAM_VERSION);
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
      printf("-h  --help          Print this output and exit.\n");
      printf("\n\n");
      exit(0);
    }
    else {
      printf("Unhandled argument: %s\n", argv[i]);
    }
  }

  scheduler = C3PScheduler::getInstance();

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
        console_adapter.poll();

        scheduler->serviceSchedules();
      } while (continue_running);   // GUI thread handles the heavy-lifting.
    }
    else {
      c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "Failed to create the root GUI window (not great, not terrible).");
    }
  }

  // Clean up any allocated stuff. It should already be hung up.
  console_adapter.poll();   // Final chance for output to make it to the user.
  platform.firmware_shutdown(0);     // Clean up the platform.
  exit(0);  // Should never execute.
}
