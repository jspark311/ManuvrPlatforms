/*
* File:   RHoM.h
* Author: J. Ian Lindsay
*
*/

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
#include "Image/ImageUtils.h"
#include "Image/GfxUI.h"
#include "Identity/Identity.h"
#include "M2MLink/M2MLink.h"
#include "CryptoBurrito/CryptoBurrito.h"
#include <Linux.h>

#ifndef __RHOM_HEADER_H__
#define __RHOM_HEADER_H__


#define PROGRAM_VERSION    "0.0.3"    // Program version.



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


// TODO: Don't code against this too much. It will probably be templated and promoted to C3P.
class LinkSockPair {
  public:
    LinuxSockPipe* sock;
    M2MLink* link;
    uint32_t established;

    LinkSockPair(LinuxSockPipe* s, M2MLink* l) :
      sock(s),
      link(l),
      established(millis())
    {};

    ~LinkSockPair() {
      delete sock;
      delete link;
    };
};


#endif  // __RHOM_HEADER_H__
