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
#include "Identity/IdentityUUID.h"
#include "Identity/Identity.h"
#include "Identity/IdentityCrypto.h"
#include "ManuvrLink/ManuvrLink.h"
#include "CryptoBurrito/CryptoBurrito.h"
#include <Linux.h>


#define PROGRAM_VERSION    "0.0.2"    // Program version.



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
