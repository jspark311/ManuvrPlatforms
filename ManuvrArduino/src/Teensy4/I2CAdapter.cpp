/*
* Teensy4
*/
#if defined(__IMXRT1052__) || defined(__IMXRT1062__)

#include <AbstractPlatform.h>
#include <I2CAdapter.h>
#include <Wire.h>


#define ACK_CHECK_EN   0x01     /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS  0x00     /*!< I2C master will not check ack from slave */



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
int8_t I2CAdapter::bus_init() {
  switch (ADAPTER_NUM) {
    case 0:
      Wire.setSDA(_bus_opts.sda_pin);
      Wire.setSCL(_bus_opts.scl_pin);
      Wire.setClock(_bus_opts.freq);
      Wire.begin();
      busOnline(true);
      break;
    case 1:
      Wire1.setSDA(_bus_opts.sda_pin);
      Wire1.setSCL(_bus_opts.scl_pin);
      Wire1.setClock(_bus_opts.freq);
      Wire1.begin();
      busOnline(true);
      break;
    default:
      break;
  }
  return (busOnline() ? 0:-1);
}


int8_t I2CAdapter::bus_deinit() {
  busOnline(false);
  switch (ADAPTER_NUM) {
    case 0:
      break;
    case 1:
      break;
    default:
      return -1;
  }
  return 0;
}



void I2CAdapter::printHardwareState(StringBuilder* output) {
  output->concatf("-- I2C%d (%sline)\n", adapterNumber(), (_adapter_flag(I2C_BUS_FLAG_BUS_ONLINE)?"on":"OFF"));
}


int8_t I2CAdapter::generateStart() {
  return busOnline() ? 0 : -1;
}


int8_t I2CAdapter::generateStop() {
  return busOnline() ? 0 : -1;
}



/*******************************************************************************
* ___     _                              These members are mandatory overrides
*  |   / / \ o     |  _  |_              from the BusOp class.
* _|_ /  \_/ o   \_| (_) |_)
*******************************************************************************/

XferFault I2CBusOp::begin() {
  uint8_t ord = 0;
  set_state(XferState::INITIATE);  // Indicate that we now have bus control.
  switch (device->adapterNumber()) {
    case 0:
      switch (get_opcode()) {
        case BusOpcode::TX_CMD:
          set_state(XferState::TX_WAIT);
          Wire.beginTransmission(dev_addr);
          if (0 != Wire.endTransmission()) {
            abort(XferFault::DEV_NOT_FOUND);
          }
          break;
        case BusOpcode::TX:
          Wire.beginTransmission(dev_addr);
          if (need_to_send_subaddr()) {
            set_state(XferState::ADDR);
            Wire.write(sub_addr);
          }
          set_state(XferState::TX_WAIT);
          while (ord < _buf_len) {
            Wire.write(*(_buf + ord++));
          }
          set_fault((0 == Wire.endTransmission()) ? XferFault::NONE : XferFault::BUS_FAULT);
          break;
        case BusOpcode::RX:
          {
            const uint8_t READ_BLOCK_SIZE = 32;
            uint8_t dev_reg = sub_addr;
            bool block_continue = true;
            while (block_continue && (ord < _buf_len)) {
              const uint8_t THIS_BLK_SIZE = strict_min(READ_BLOCK_SIZE, (_buf_len - ord));
              uint8_t btr = 0;
              if (sub_addr != -1) {
                Wire.beginTransmission(dev_addr);
                Wire.write(dev_reg);
                Wire.endTransmission(false);
              }
              Wire.requestFrom(dev_addr, THIS_BLK_SIZE);
              while (Wire.available()) {
                *(_buf + ord++) = Wire.read();
                btr++;
              }
              block_continue = (0 == Wire.endTransmission()) && (btr == THIS_BLK_SIZE);
              //block_continue = (btr == THIS_BLK_SIZE);
              dev_reg += THIS_BLK_SIZE;
            }
            set_fault((_buf_len == ord) ? XferFault::NONE : XferFault::BUS_FAULT);
          }
          break;
        default:
          abort(XferFault::BAD_PARAM);
          break;
      }
      break;

    case 1:
      switch (get_opcode()) {
        case BusOpcode::TX_CMD:
          set_state(XferState::TX_WAIT);
          Wire1.beginTransmission(dev_addr);
          if (0 != Wire1.endTransmission()) {
            abort(XferFault::DEV_NOT_FOUND);
          }
          break;
        case BusOpcode::TX:
          Wire1.beginTransmission(dev_addr);
          if (need_to_send_subaddr()) {
            set_state(XferState::ADDR);
            Wire1.write(sub_addr);
          }
          set_state(XferState::TX_WAIT);
          while (ord < _buf_len) {
            Wire1.write(*(_buf + ord++));
          }
          set_fault((0 == Wire1.endTransmission()) ? XferFault::NONE : XferFault::BUS_FAULT);
          break;
        case BusOpcode::RX:
          {
            const uint8_t READ_BLOCK_SIZE = 32;
            uint8_t dev_reg = sub_addr;
            bool block_continue = true;
            while (block_continue && (ord < _buf_len)) {
              const uint8_t THIS_BLK_SIZE = strict_min(READ_BLOCK_SIZE, (_buf_len - ord));
              uint8_t btr = 0;
              if (sub_addr != -1) {
                Wire1.beginTransmission(dev_addr);
                Wire1.write(dev_reg);
                Wire1.endTransmission(false);
              }
              Wire1.requestFrom(dev_addr, THIS_BLK_SIZE);
              while (Wire1.available()) {
                *(_buf + ord++) = Wire1.read();
                btr++;
              }
              block_continue = (0 == Wire1.endTransmission()) && (btr == THIS_BLK_SIZE);
              //block_continue = (btr == THIS_BLK_SIZE);
              dev_reg += THIS_BLK_SIZE;
            }
            set_fault((_buf_len == ord) ? XferFault::NONE : XferFault::BUS_FAULT);
          }
          break;
        default:
          abort(XferFault::BAD_PARAM);
          break;
      }
      break;
    default:
      abort(XferFault::BAD_PARAM);
      break;
  }
  markComplete();
  return getFault();
}




/*
* FreeRTOS doesn't have a concept of interrupt, but we might call this
*   from an I/O thread.
*/
XferFault I2CBusOp::advance(uint32_t status_reg) {
  return getFault();
}



#endif // defined(__IMXRT1052__) || defined(__IMXRT1062__)
