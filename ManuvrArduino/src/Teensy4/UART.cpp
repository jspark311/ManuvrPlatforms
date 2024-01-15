#include <BusQueue/UARTAdapter.h>


static HardwareSerial* _uart_get_by_adapter_num(const uint8_t A_NUM) {
  switch (A_NUM) {
    case 1:   return &Serial1;
    case 2:   return &Serial2;
    case 3:   return &Serial3;
    case 4:   return &Serial4;
    case 5:   return &Serial5;
    case 6:   return &Serial6;
    case 7:   return &Serial7;
    default:  return nullptr;
  }
}


/*
* In-class ISR handler. Be careful about state mutation....
*/
void UARTAdapter::irq_handler() {
}


/**
* Execute any I/O callbacks that are pending. The function is present because
*   this class contains the bus implementation.
*
* @return 0 on success.
*/
int8_t UARTAdapter::poll() {
  int8_t return_value = 0;
  HardwareSerial* s_port = _uart_get_by_adapter_num(ADAPTER_NUM);

  if (txCapable() && (0 < _tx_buffer.count())) {
    // Refill the TX buffer...
    const uint32_t TX_COUNT = strict_min((uint32_t) 64, (uint32_t) _tx_buffer.count());
    if (0 < TX_COUNT) {
      uint8_t side_buffer[TX_COUNT] = {0};
      const int32_t PEEK_COUNT = _tx_buffer.peek(side_buffer, TX_COUNT);
      //const int32_t BYTES_WRITTEN = uart_write_bytes((uart_port_t) ADAPTER_NUM, (const char*) side_buffer, (size_t) PEEK_COUNT);
      switch (ADAPTER_NUM) {
        case 0:
          if (Serial) {  Serial.write(side_buffer, TX_COUNT);   }
          break;
        default:
          if (s_port) {  s_port->write(side_buffer, TX_COUNT);  }
          break;
      }
      if (TX_COUNT > 0) {
        _tx_buffer.cull(TX_COUNT);
      }
      _flushed = _tx_buffer.isEmpty();
    }
  }
  if (rxCapable()) {
    const uint8_t RX_BUF_LEN = 64;
    uint8_t ser_buffer[RX_BUF_LEN] = {0, };
    uint8_t rx_len = 0;
    switch (ADAPTER_NUM) {
      case 0:
        if (Serial) {
          while ((RX_BUF_LEN > rx_len) && (0 < Serial.available())) {
            ser_buffer[rx_len++] = Serial.read();
          }
        }
        break;

      default:
        if (s_port) {
          while ((RX_BUF_LEN > rx_len) && (0 < s_port->available())) {
            ser_buffer[rx_len++] = s_port->read();
          }
        }
        break;
    }
    if (rx_len > 0) {
      _rx_buffer.insert(ser_buffer, rx_len);
    }
    _handle_rx_push();
  }
  return return_value;
}


int8_t UARTAdapter::_pf_init() {
  int8_t ret = -1;
  HardwareSerial* s_port = _uart_get_by_adapter_num(ADAPTER_NUM);
  switch (ADAPTER_NUM) {
    case 0:
      Serial.begin(_opts.bitrate);   // USB
      _adapter_set_flag(UART_FLAG_HAS_TX | UART_FLAG_HAS_RX);
      _adapter_set_flag(UART_FLAG_UART_READY);
      _adapter_clear_flag(UART_FLAG_PENDING_CONF | UART_FLAG_PENDING_RESET);
      _flushed = true;
      ret = 0;
      break;

    default:   // Hardware UARTs.
      s_port->begin(_opts.bitrate);
      s_port->setTX(_TXD_PIN);
      s_port->setRX(_RXD_PIN);
      if (255 != _RTS_PIN) {   s_port->attachRts(_RTS_PIN);  }
      if (255 != _CTS_PIN) {   s_port->attachCts(_CTS_PIN);  }
      _adapter_set_flag(UART_FLAG_HAS_RX, (255 != _RXD_PIN));
      _adapter_set_flag(UART_FLAG_HAS_TX, (255 != _TXD_PIN));
      _adapter_set_flag(UART_FLAG_UART_READY);
      _flushed = true;
      _adapter_clear_flag(UART_FLAG_PENDING_CONF | UART_FLAG_PENDING_RESET);
      ret = 0;
      break;
  }
  return ret;
}


int8_t UARTAdapter::_pf_deinit() {
  int8_t ret = -2;
  _adapter_clear_flag(UART_FLAG_UART_READY | UART_FLAG_PENDING_RESET | UART_FLAG_PENDING_CONF);
  HardwareSerial* s_port = _uart_get_by_adapter_num(ADAPTER_NUM);
  if (txCapable()) {
    switch (ADAPTER_NUM) {
      case 0:
        Serial.flush();   // USB
        break;

      default:   // Hardware UARTs.
        s_port->flush();
        break;
    }
    _flushed = true;
    ret = 0;
  }
  return ret;
}
