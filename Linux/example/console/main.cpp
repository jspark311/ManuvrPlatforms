/*
File:   main.cpp
Author: J. Ian Lindsay
Date:   2016.08.26

Copyright 2016 Manuvr, Inc

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

This is a demonstration program, and was meant to be compiled for a
  linux target.
*/


/* CppPotpourri */
#include <StringBuilder.h>
#include <CppPotpourri.h>
#include <ParsingConsole.h>

/* ManuvrPlatform for vanilla Linux. */
#include <Linux.h>


/*******************************************************************************
* Defs and types                                                               *
*******************************************************************************/
#define PROGRAM_VERSION        "0.0.4"    // Program version.
#define MAX_COMMAND_LENGTH         512    // The maximum size of user input.


/*******************************************************************************
* Globals                                                                      *
*******************************************************************************/

using namespace std;

const char* program_name;
int   continue_running  = 1;

/* Console support... */
ParsingConsole console(MAX_COMMAND_LENGTH);
LinuxStdIO console_adapter;


/*******************************************************************************
* Console callbacks
*******************************************************************************/

int callback_help(StringBuilder* text_return, StringBuilder* args) {
  text_return->concatf("%s %s\n", program_name, PROGRAM_VERSION);
  if (0 < args->count()) {
    console.printHelp(text_return, args->position_trimmed(0));
  }
  else {
    console.printHelp(text_return);
  }
  return 0;
}


int callback_console_tools(StringBuilder* text_return, StringBuilder* args) {
  //inline void setPromptString(const char* str) {    _prompt_string = (char*) str;   };
  //inline bool hasColor() {               return _console_flag(CONSOLE_FLAG_HAS_ANSI);                   };
  //inline void hasColor(bool x) {         return _console_set_flag(CONSOLE_FLAG_HAS_ANSI, x);            };
  int ret = 0;
  char* cmd    = args->position_trimmed(0);
  int   arg1   = args->position_as_int(1);
  bool  print_term_enum = false;
  if (0 == StringBuilder::strcasecmp(cmd, "echo")) {
    if (1 < args->count()) {
      console.localEcho(0 != arg1);
    }
    text_return->concatf("Console RX echo %sabled.\n", console.localEcho()?"en":"dis");
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "history")) {
    if (1 < args->count()) {
      console.emitPrompt(0 != arg1);
      char* subcmd = args->position_trimmed(1);
      if (0 == StringBuilder::strcasecmp(subcmd, "clear")) {
        console.clearHistory();
        text_return->concat("History cleared.\n");
      }
      else if (0 == StringBuilder::strcasecmp(subcmd, "depth")) {
        if (2 < args->count()) {
          arg1 = args->position_as_int(2);
          console.maxHistoryDepth(arg1);
        }
        text_return->concatf("History depth: %u\n", console.maxHistoryDepth());
      }
      else if (0 == StringBuilder::strcasecmp(subcmd, "logerrors")) {
        if (2 < args->count()) {
          arg1 = args->position_as_int(2);
          console.historyFail(0 != arg1);
        }
        text_return->concatf("History %scludes failed commands.\n", console.historyFail()?"in":"ex");
      }
      else text_return->concat("Valid options are [clear|depth|logerrors]\n");
    }
    else console.printHistory(text_return);
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "help-on-fail")) {
    if (1 < args->count()) {
      console.printHelpOnFail(0 != arg1);
    }
    text_return->concatf("Console prints command help on failure: %s.\n", console.printHelpOnFail()?"yes":"no");
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "prompt")) {
    if (1 < args->count()) {
      console.emitPrompt(0 != arg1);
    }
    text_return->concatf("Console autoprompt %sabled.\n", console.emitPrompt()?"en":"dis");
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "force")) {
    if (1 < args->count()) {
      console.forceReturn(0 != arg1);
    }
    text_return->concatf("Console force-return %sabled.\n", console.forceReturn()?"en":"dis");
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "rxterm")) {
    if (1 < args->count()) {
      switch (arg1) {
        case 0:  case 1:  case 2:  case 3:
          console.setRXTerminator((LineTerm) arg1);
          break;
        default:
          print_term_enum = true;
          break;
      }
    }
    text_return->concatf("Console RX terminator: %s\n", ParsingConsole::terminatorStr(console.getRXTerminator()));
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "txterm")) {
    if (1 < args->count()) {
      switch (arg1) {
        case 0:  case 1:  case 2:  case 3:
          console.setTXTerminator((LineTerm) arg1);
          break;
        default:
          print_term_enum = true;
          break;
      }
    }
    text_return->concatf("Console TX terminator: %s\n", ParsingConsole::terminatorStr(console.getTXTerminator()));
  }
  else {
    ret = -1;
  }

  if (print_term_enum) {
    text_return->concat("Terminator options:\n");
    text_return->concat("\t0: ZEROBYTE\n");
    text_return->concat("\t1: CR\n");
    text_return->concat("\t2: LF\n");
    text_return->concat("\t3: CRLF\n");
  }
  return ret;
}


int callback_kvp_tools(StringBuilder* text_return, StringBuilder* args) {
  int ret = 0;
  char* cmd = args->position_trimmed(0);
  return ret;
}


int callback_program_quit(StringBuilder* text_return, StringBuilder* args) {
  continue_running = 0;
  console.emitPrompt(false);  // Avoid a trailing prompt.
  text_return->concat("Stopping...\n");
  return 0;
}



/*******************************************************************************
* The main function.                                                           *
*******************************************************************************/
int main(int argc, const char* argv[]) {
  program_name = argv[0];  // Our name.
  StringBuilder output;    // Most programs will want a logging buffer.

  /*
  * The platform object is created on the stack, but takes no action upon
  *   construction. The first thing that should be done is to call the init
  *   function to setup the defaults of the platform.
  */
  platform.init();

  /*
  * At this point, we should configure our console and define commands.
  */
  console.setTXTerminator(LineTerm::LF);
  console.setRXTerminator(LineTerm::LF);
  console.localEcho(false);   // This happens naturaly to STDIO.

  // Mutually connect the console class to STDIO.
  console_adapter.readCallback(&console);
  console.setOutputTarget(&console_adapter);

  // We want to have a nice prompt string. Note that the console does not make
  //   a copy of the string we provide it. So we need to keep its pointer
  //   constant until...
  //     1) the console is torn down.
  //     2) we change it with another call to setPromptString().
  //     3) we call emitPrompt(false), to prevent it from being used.
  StringBuilder prompt_string;
  prompt_string.concatf("%c[36m%s> %c[39m", 0x1B, program_name, 0x1B);
  console.setPromptString((const char*) prompt_string.string());
  console.emitPrompt(true);
  console.hasColor(true);

  // Define the commands for the application. Usually, these are some basics.
  console.defineCommand("help",     '?', ParsingConsole::tcodes_str_1, "Prints help to console.", "", 0, callback_help);
  console.defineCommand("console",  '\0', ParsingConsole::tcodes_str_3, "Console conf", "[history|rxterm|txterm|echo|prompt]", 0, callback_console_tools);
  console.defineCommand("kvp",         'k', ParsingConsole::tcodes_str_4, "Temporary code to test KVP.", "", 0, callback_kvp_tools);
  console.defineCommand("quit",     'Q', ParsingConsole::tcodes_0, "Commit sudoku.", "", 0, callback_program_quit);

  // The platform itself comes with a convenient set of console functions.
  platform.configureConsole(&console);

  console.init();

  output.concatf("%s initialized.\n", argv[0]);
  console.printToLog(&output);
  console.printPrompt();

  /*
  * The main loop. Run until told to stop.
  */
  while (continue_running) {
    // Polling the adapter will drive the entire program forward.
    console_adapter.poll();
  }

  platform.firmware_shutdown(0);     // Clean up the platform.
}
