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

#include "C3POnX11.h"
#include "C3PLinux.h"
#include "WorldCrafter.h"


extern void* gui_thread_handler(void*);
int callback_gui_tools(StringBuilder*, StringBuilder*);

using namespace std;


/*******************************************************************************
* Globals
*******************************************************************************/
const char*   program_name;
bool          continue_running  = true;

MainGuiWindow* c3p_root_window   = nullptr;

LinuxStdIO console_adapter;
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
    MainGuiWindow c3p_root_window(0, 0, 1600, 1080, argv[0]);
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
