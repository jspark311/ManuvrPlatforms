/* Compiler */
#include <inttypes.h>
#include <stdint.h>
#include <math.h>

/* ManuvrPlatform */
#include <ESP32.h>

/* CppPotpourri */
#include "CppPotpourri.h"
#include "AbstractPlatform.h"
#include "StringBuilder.h"
#include "Console/C3PConsole.h"
#include "TimerTools/TimerTools.h"
#include "uuid.h"
#include "Identity/Identity.h"
#include "Identity/IdentityUUID.h"
#include "BusQueue/UARTAdapter.h"
#include "cbor-cpp/cbor.h"

/* ManuvrDrivers */
#include "ManuvrDrivers.h"

#ifndef __WIFICOMMUNIT_H__
#define __WIFICOMMUNIT_H__

// TODO: I _HaTe* that I have replicated this awful pattern of hard-coded
//   program versions (which are never updated) into so many projects. Finally
//   decide on a means of doing this that more-closely resembles the awesome
//   arrangement that I have at LTi for automatically binding the firmware
//   version to source-control.
#define TEST_PROG_VERSION           "1.0"


/*******************************************************************************
* Pin definitions and hardware constants.
*******************************************************************************/
/* Platform pins */
#define UART2_TX_PIN       18   // OUTPUT
#define UART2_RX_PIN       19   // INPUT_PULLUP
#define LED_R_PIN          25   // OUTPUT Active low
#define LED_G_PIN          26   // OUTPUT Active low


/*******************************************************************************
* Invariant software parameters
*******************************************************************************/

/*******************************************************************************
* Types
*******************************************************************************/

/*******************************************************************************
* Externs to hardware resources
*******************************************************************************/

/*******************************************************************************
* Externs to hardware resources
*******************************************************************************/

/*******************************************************************************
* Externs to software singletons
*******************************************************************************/


#endif    // __WIFICOMMUNIT_H__
