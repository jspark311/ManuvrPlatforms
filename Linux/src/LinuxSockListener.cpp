/*
File:   LinuxSockListener.cpp
Author: J. Ian Lindsay
Date:   2022.06.26

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


This class should be polled regularly, and will produce LinuxSockPipes as
  connections come in.
*/


#include <Linux.h>

#if defined(CONFIG_C3P_SOCKET_WRAPPER)

#include <cstdio>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>


/*******************************************************************************
*      _______.___________.    ___   .___________. __    ______     _______.
*     /       |           |   /   \  |           ||  |  /      |   /       |
*    |   (----`---|  |----`  /  ^  \ `---|  |----`|  | |  ,----'  |   (----`
*     \   \       |  |      /  /_\  \    |  |     |  | |  |        \   \
* .----)   |      |  |     /  _____  \   |  |     |  | |  `----.----)   |
* |_______/       |__|    /__/     \__\  |__|     |__|  \______|_______/
*
* Static members and initializers should be located here.
*******************************************************************************/

/**
* This is a thread to keep the sockets churning.
*/
static void* socket_listener_polling_handler(void* active_xport) {
  LinuxSockListener* xport = (LinuxSockListener*) active_xport;
  c3p_log(LOG_LEV_DEBUG, __PRETTY_FUNCTION__, "Started socket listener thread.");
  sigset_t set;
  sigemptyset(&set);
  //sigaddset(&set, SIGIO);
  sigaddset(&set, SIGQUIT);
  sigaddset(&set, SIGHUP);
  sigaddset(&set, SIGTERM);
  sigaddset(&set, SIGVTALRM);
  sigaddset(&set, SIGINT);
  int s = pthread_sigmask(SIG_BLOCK, &set, NULL);

  if (0 == s) {
    while (xport->listening()) {
      xport->poll();
      sleep_ms(20);
    }
  }
  else {
    //output.concatf("pthread_sigmask() returned an error: %d\n", s);
  }

  c3p_log(LOG_LEV_DEBUG, __PRETTY_FUNCTION__, "Exiting socket listener thread...");
  return nullptr;
}


/*******************************************************************************
* Socket listener wrapper class
*******************************************************************************/

/**
* Constructor will allocate memory for its lookup list item and the path string.
* Adds itself to the lookup list.
*
* NOTE: Because this constructor modifies static data, it can't be relied upon
*   if an instance of it is ever allocated statically. So don't do that.
*/
LinuxSockListener::LinuxSockListener(char* path) {
  _set_sock_path(path);
}


/**
* Destructor will de-init the hardware, remove itself from the lookup list,
*   and free any memory it used to store itself.
*/
LinuxSockListener::~LinuxSockListener() {
  close();
  if (_sock_path) {
    free(_sock_path);
    _sock_path = nullptr;
  }
}



/**
* Execute any I/O callbacks that are pending. The function is present because
*   this class contains the bus implementation.
*
* @return 0 or greater on success.
*/
int8_t LinuxSockListener::poll() {
  int8_t ret = 0;
  if (_sock_id > 0) {
    int      cli_sock;
    struct sockaddr_in cli_addr;
    unsigned int clientlen = sizeof(cli_addr);

    /* Wait for client connection */
    if ((cli_sock = accept(_sock_id, (struct sockaddr *) &cli_addr, &clientlen)) < 0) {
      //output.concat("Failed to accept client connection.\n");
    }
    else {
      //output.concatf("TCP Client connected: %s\n", (char*) inet_ntoa(cli_addr.sin_addr));
      if (nullptr != _new_cb) {
        LinuxSockPipe* nu_connection = new LinuxSockPipe(_sock_path, cli_sock);
        if (1 != _new_cb(this, nu_connection)) {
          delete nu_connection;
        }
      }
    }
  }
  return ret;
}



int8_t LinuxSockListener::close() {
  int8_t ret = -1;
  if (0 < _sock_id) {
    ::close(_sock_id);  // Close the socket.
    c3p_log(LOG_LEV_NOTICE, __PRETTY_FUNCTION__, "Closed listener socket %d (%s)", _sock_id, ((_sock_path) ? _sock_path : "no path"));
    _sock_id = -1;
    ret = 0;
  }
  return ret;
}



// Returns the number of connections, or -1 if not listening.
int LinuxSockListener::listening() {
  int ret = 0;
  if (_sock_id > 0) {
    ret++;
  }
  return ret;
}


int LinuxSockListener::listen(char* path) {
  int ret = 0;

  if (!listening()) {
    char* conn_sock = _sock_path;
    if (nullptr != path) {
      if (0 != _set_sock_path(path)) {
        conn_sock = (char*) "";
      }
    }
    else if (nullptr == _sock_path) {
      conn_sock = (char*) "";
    }

    if (0 < strlen(conn_sock)) {   // We have something to work with.
      
      ret = 0;
    }
    else {  // No action is possible. We haven't been given a path.
      ret = -1;
    }
  }

  if (_sock_id > 0) {  ret++;    }
  return ret;
}



int8_t LinuxSockListener::_set_sock_path(char* path) {
  int8_t ret = -1;
  if (!listening()) {
    ret--;
    if (path) {
      ret--;
      const int slen = strlen(path);
      if (slen > 0) {
        ret--;
        if (_sock_path) {
          if (0 != StringBuilder::strcasecmp(path, _sock_path)) {   // TODO: should be a case-sensitive test.
            free(_sock_path);
            _sock_path = nullptr;
          }
          else {
            // The given path is the same as what we already have. Do nothing.
            return 0;
          }
        }

        _sock_path = (char*) malloc(slen+1);
        if (_sock_path) {
          memcpy(_sock_path, path, slen);
          *(_sock_path + slen) = '\0';
          ret = 0;
        }
      }
    }
  }
  return ret;
}


void LinuxSockListener::printDebug(StringBuilder* output) {
  StringBuilder temp("Socket Listener");
  temp.concatf("%s (%slistening", _sock_path, ((0 < listening()) ? "":"not "));
  if (0 < listening()) {
    temp.concatf(": %d)", _sock_id);
  }
  else {
    temp.concat(")");
  }
  StringBuilder::styleHeader1(output, (char*) temp.string());
}


/*******************************************************************************
* Console callback
* These are built-in handlers for using this instance via a console.
*******************************************************************************/

/**
* @page console-handlers
* @section socket-tools Socket Listener tools
*
* This is the console handler for debugging the operation of `LinuxSockListener`'s.
*
*/
int8_t LinuxSockListener::console_handler(StringBuilder* text_return, StringBuilder* args) {
  int ret = 0;
  char* cmd = args->position_trimmed(0);

  if (0 == StringBuilder::strcasecmp(cmd, "listen")) {
    int ret_local = 0;
    if (args->count() > 1) {
      ret_local = listen(args->position_trimmed(1));
    }
    else {
      ret_local = listen();
    }
    text_return->concatf("listen(%s) returned %d\n", _sock_path, ret_local);
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "close")) {
    text_return->concatf("close() returned %d\n", close());
  }
  else if (0 == StringBuilder::strcasecmp(cmd, "poll")) {
    text_return->concatf("poll() returned %d\n", poll());
  }
  else {
    printDebug(text_return);
  }

  return ret;
}

#endif   // CONFIG_C3P_SOCKET_WRAPPER
