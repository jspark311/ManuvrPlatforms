/*
File:   SPIAdapter.cpp
Author: J. Ian Lindsay
Date:   2016.12.17

Hardware-specific SPI implementation for Teensy4.
*/

#if defined(__IMXRT1052__) || defined(__IMXRT1062__)

#include <AbstractPlatform.h>
#include <BusQueue/SPIAdapter.h>
#include <SPI.h>

static uint8_t clock_mode = SPI_MODE0;


/*******************************************************************************
* BusOp functions below...
*******************************************************************************/

/**
* Calling this member will cause the bus operation to be started.
*
* @return 0 on success, or non-zero on failure.
*/
XferFault SPIBusOp::begin() {
  XferFault ret = XferFault::IO_RECALL;
  // NOTE: Short-circuit eval and the order of args to the if() is very important to not crash.
  if ((nullptr == callback) || (0 == callback->io_op_callahead(this))) {
    set_state(XferState::INITIATE);  // Indicate that we now have bus control.
    SPISettings spi_settings(_bus->frequency(), MSBFIRST, clock_mode);
    SPI.beginTransaction(spi_settings);
    _assert_cs(true);
    set_state(XferState::ADDR);

    if (_param_len) {
      for (uint8_t i = 0; i < _param_len; i++) {
        SPI.transfer(xfer_params[i]);
      }
    }

    if (_buf_len) {
      switch (get_opcode()) {
        case BusOpcode::TX:
          set_state(XferState::TX_WAIT);
          for (uint32_t i = 0; i < _buf_len; i++) {
            SPI.transfer(*(_buf + i));
          }
          //SPI.transfer(_buf, nullptr, _buf_len, EventResponderRef event_responder);
          ret = XferFault::NONE;
          break;
        case BusOpcode::RX:
          set_state(XferState::RX_WAIT);
          for (uint32_t i = 0; i < _buf_len; i++) {
            *(_buf + i) = SPI.transfer(0);
          }
          ret = XferFault::NONE;
          break;
        default:
          ret = XferFault::BAD_PARAM;
          break;
      }
    }
    SPI.endTransaction();
  }
  markComplete();
  return ret;
}


/**
* Called from the ISR to advance this operation on the bus.
* Stay brief. We are in an ISR.
*
* @return 0 on success. Non-zero on failure.
*/
int8_t SPIBusOp::advance_operation(uint32_t status_reg, uint8_t data_reg) {
  /* These are our transfer-size-invariant cases. */
  switch (get_state()) {
    case XferState::COMPLETE:
      abort(XferFault::HUNG_IRQ);
      return 0;

    case XferState::TX_WAIT:
    case XferState::RX_WAIT:
      markComplete();
      return 0;

    case XferState::FAULT:
      return 0;

    case XferState::QUEUED:
    case XferState::ADDR:
    case XferState::STOP:
    case XferState::UNDEF:

    /* Below are the states that we shouldn't be in at this point... */
    case XferState::INITIATE:
    case XferState::IDLE:
      abort(XferFault::ILLEGAL_STATE);
      return 0;
  }

  return -1;
}



/*******************************************************************************
* ___     _       _                      These members are mandatory overrides
*  |   / / \ o   | \  _     o  _  _      for implementing I/O callbacks. They
* _|_ /  \_/ o   |_/ (/_ \/ | (_ (/_     are also implemented by Adapters.
*******************************************************************************/
/**
* Called prior to the given bus operation beginning.
* Returning 0 will allow the operation to continue.
* Returning anything else will fail the operation with IO_RECALL.
*   Operations failed this way will have their callbacks invoked as normal.
*
* @param  _op  The bus operation that was completed.
* @return 0 to run the op, or non-zero to cancel it.
*/
int8_t SPIAdapter::io_op_callahead(BusOp* _op) {
  // Bus adapters don't typically do anything here, other
  //   than permit the transfer.
  return 0;
}

/**
* When a bus operation completes, it is passed back to its issuing class.
*
* @param  _op  The bus operation that was completed.
* @return BUSOP_CALLBACK_NOMINAL on success, or appropriate error code.
*/
int8_t SPIAdapter::io_op_callback(BusOp* _op) {
  return BUSOP_CALLBACK_NOMINAL;
}


/*******************************************************************************
* ___     _                                  This is a template class for
*  |   / / \ o    /\   _|  _. ._ _|_  _  ._  defining arbitrary I/O adapters.
* _|_ /  \_/ o   /--\ (_| (_| |_) |_ (/_ |   Adapters must be instanced with
*                             |              a BusOp as the template param.
*******************************************************************************/
void SPIAdapter::printDebug(StringBuilder* output) {
  printAdapter(output);
  printHardwareState(output);
}

void SPIAdapter::printHardwareState(StringBuilder* output) {
}


FAST_FUNC int8_t SPIAdapter::frequency(const uint32_t f) {
  int8_t ret = -1;
  if (0 < f) {
    ret--;
    if (2 > ADAPTER_NUM) {
      _current_freq = f;
      ret = 0;
    }
  }
  return ret;
}


FAST_FUNC int8_t SPIAdapter::setMode(const uint8_t m) {
  int8_t   ret = -2;
  if (2 > ADAPTER_NUM) {
    ret = 0;
    switch (m) {
      case 0:   clock_mode = SPI_MODE0;   break;
      case 1:   clock_mode = SPI_MODE1;   break;
      case 2:   clock_mode = SPI_MODE2;   break;
      case 3:   clock_mode = SPI_MODE3;   break;
      default:  ret = -3;   break;
    }
  }
  return ret;
}


int8_t SPIAdapter::_bus_init() {
  int8_t ret = 0;
  SPI.setSCK(_CLK_PIN);
  SPI.setMISO(_MISO_PIN);
  SPI.setMOSI(_MOSI_PIN);
  SPI.begin();
  return ret;
}


int8_t SPIAdapter::_bus_deinit() {
  return 0;
}

#endif // defined(__IMXRT1052__) || defined(__IMXRT1062__)
