/*
* Teensy4
*/
#if defined(__IMXRT1052__) || defined(__IMXRT1062__)

#include <AbstractPlatform.h>
#include <BusQueue/I2CAdapter.h>
#include "ManuvrArduino.h"
#include "imx_rt1060/imx_rt1060_i2c_driver.h"


/*******************************************************************************
* The code under this block is special on this platform,
*   and will not be available elsewhere.
*******************************************************************************/

I2CMaster& master0 = Master;     // Pins 19 and 18; SCL0 and SDA0
I2CMaster& master1 = Master1;    // Pins 16 and 17; SCL1 and SDA1

uint8_t _dead_buf[2][256] = {0, };


/*******************************************************************************
* ___     _                                  This is a template class for
*  |   / / \ o    /\   _|  _. ._ _|_  _  ._  defining arbitrary I/O adapters.
* _|_ /  \_/ o   /--\ (_| (_| |_) |_ (/_ |   Adapters must be instanced with
*                             |              a BusOp as the template param.
*******************************************************************************/

/*
* Init the hardware for the bus.
* There is only one correct pin combination for each i2c bus (surprisingly).
*/
int8_t I2CAdapter::_bus_init() {
  int8_t ret = -1;
  switch (ADAPTER_NUM) {
    case 0:
      if ((18 == _bus_opts.sda_pin) && (19 == _bus_opts.scl_pin)) {
        master0.end();
        master0.begin(_bus_opts.freq);
        _pf_needs_op_advance(true);
        ret = 0;
      }
      break;
    case 1:
      if ((17 == _bus_opts.sda_pin) && (16 == _bus_opts.scl_pin)) {
        master1.end();
        master1.begin(_bus_opts.freq);
        _pf_needs_op_advance(true);
        ret = 0;
      }
      break;
    default:
      break;
  }
  return ret;
}


int8_t I2CAdapter::_bus_deinit() {
  _bus_online(false);
  switch (ADAPTER_NUM) {
    case 0:   master0.end();   break;
    case 1:   master1.end();   break;
    default:  return -1;
  }
  return 0;
}


void I2CAdapter::printHardwareState(StringBuilder* output) {
  output->concatf("-- I2C%d (%sline)\n", adapterNumber(), (busOnline()?"on":"OFF"));
}



/*******************************************************************************
* ___     _                              These members are mandatory overrides
*  |   / / \ o     |  _  |_              from the BusOp class.
* _|_ /  \_/ o   \_| (_) |_)
*******************************************************************************/

XferFault I2CBusOp::begin() {
  I2CMaster* adptr = nullptr;
  uint8_t ord = 0;
  switch (device->adapterNumber()) {
    case 0:    adptr = &master0;             break;
    case 1:    adptr = &master1;             break;
    default:   abort(XferFault::BAD_PARAM);  return getFault();
  }
  if (!adptr->finished()) {
    return XferFault::BUS_BUSY;
  }
  set_state(XferState::INITIATE);  // Indicate that we now have bus control.

  switch (get_opcode()) {
    case BusOpcode::TX_CMD:
      set_state(XferState::TX_WAIT);
      adptr->write_async(dev_addr, &_dead_buf[device->adapterNumber()][0], 0, 1);
      break;

    case BusOpcode::TX:
      if (need_to_send_subaddr()) {
        set_state(XferState::TX_WAIT);
        _dead_buf[device->adapterNumber()][0] = (uint8_t) sub_addr;
        for (uint32_t x = 0; x < strict_min(255, _buf_len); x++) {
          _dead_buf[device->adapterNumber()][x+1] = _buf[x];
        }
        adptr->write_async(dev_addr, &_dead_buf[device->adapterNumber()][0], (_buf_len+1), 1);
      }
      else {
        set_state(XferState::TX_WAIT);
        adptr->write_async(dev_addr, _buf, _buf_len, 1);
      }
      break;

    case BusOpcode::RX:
      if (need_to_send_subaddr()) {
        set_state(XferState::ADDR);
        adptr->write_async(dev_addr, (uint8_t*) &sub_addr, 1, 0);
      }
      else {
        set_state(XferState::RX_WAIT);
        adptr->read_async(dev_addr, _buf, _buf_len, 1);
      }
      break;

    default:
      abort(XferFault::BAD_PARAM);
      break;
  }
  return getFault();
}



/*
*/
XferFault I2CBusOp::advance(uint32_t status_reg) {
  I2CMaster* adptr = nullptr;
  switch (device->adapterNumber()) {
    case 0:    adptr = &master0;             break;
    case 1:    adptr = &master1;             break;
    default:   abort(XferFault::BAD_PARAM);  return getFault();
  }

  switch (adptr->error()) {
    case I2CError::ok:
      if (adptr->finished()) {
        switch (get_state()) {
          case XferState::UNDEF:
          case XferState::IDLE:
          case XferState::QUEUED:
          case XferState::INITIATE:
            break;
          case XferState::ADDR:
            switch (get_opcode()) {
              case BusOpcode::RX:
                if (adptr->get_bytes_transferred() == 1) {
                  set_state(XferState::RX_WAIT);
                  adptr->read_async(dev_addr, _buf, _buf_len, 1);
                }
                else {
                  abort(XferFault::DEV_NOT_FOUND);
                }
                break;
              case BusOpcode::TX:
                if (adptr->get_bytes_transferred() == 1) {
                  set_state(XferState::TX_WAIT);
                  adptr->write_async(dev_addr, _buf, _buf_len, 1);
                }
                else {
                  abort(XferFault::DEV_NOT_FOUND);
                }
                break;
              case BusOpcode::TX_CMD:
                set_state(XferState::TX_WAIT);
                adptr->write_async(dev_addr, &_dead_buf[device->adapterNumber()][0], 0, 1);
                break;
              default:
                abort(XferFault::ILLEGAL_STATE);
                break;
            }
            break;
          case XferState::TX_WAIT:
          case XferState::RX_WAIT:
            switch (get_opcode()) {
              case BusOpcode::RX:
                if (adptr->get_bytes_transferred() == _buf_len) {
                  markComplete();
                }
                else {
                  abort(XferFault::BUS_FAULT);
                }
                break;
              case BusOpcode::TX:
                if (adptr->get_bytes_transferred() == (_buf_len+(need_to_send_subaddr() ? 1 : 0))) {
                  markComplete();
                }
                else {
                  abort(XferFault::BUS_FAULT);
                }
                break;
              case BusOpcode::TX_CMD:
                markComplete();
                break;
              default:
                abort(XferFault::ILLEGAL_STATE);
                break;
            }
            break;
          case XferState::STOP:
            markComplete();
            break;
          case XferState::COMPLETE:
          case XferState::FAULT:
          default:
            break;
        }
      }
      break;
    case I2CError::master_not_ready:       // Caller failed to wait for one transaction to finish before starting the next
    case I2CError::arbitration_lost:       // Another master interrupted
      abort(XferFault::BUS_BUSY);
      break;
    case I2CError::buffer_overflow:        // Not enough room in receive buffer to hold all the data. Bytes dropped.
    case I2CError::buffer_underflow:       // Not enough data in transmit buffer to satisfy reader. Padding sent.
    case I2CError::invalid_request:        // Caller asked Master to read more than 256 bytes in one go
      abort(XferFault::DMA_FAULT);
      break;
    case I2CError::master_pin_low_timeout: // SCL or SDA held low for too long. Can be caused by a stuck slave.
      abort(XferFault::TIMEOUT);
      break;
    case I2CError::master_fifo_error:      // Master attempted to send or receive without a START. Programming error.
    case I2CError::master_fifos_not_empty: // Programming error. FIFOs not empty at start of transaction.
      abort(XferFault::ILLEGAL_STATE);
      break;
    case I2CError::address_nak:
      abort(XferFault::DEV_NOT_FOUND);
      break;
    case I2CError::data_nak:
      abort(XferFault::DEV_FAULT);
      break;
    case I2CError::bit_error:
      abort(XferFault::UNDEFD_REGISTER);
      break;
    default:
      abort(XferFault::NO_REASON);
      break;
  }
  return getFault();
}

#endif // defined(__IMXRT1052__) || defined(__IMXRT1062__)
