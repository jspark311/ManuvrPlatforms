#include <UARTAdapter.h>

UARTAdapter* uart_instances[] = {nullptr, nullptr, nullptr};


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

  if (txCapable() && (0 < _tx_buffer.length())) {
    // Refill the TX buffer...
    int tx_count = strict_min((int32_t) 64, (int32_t) _tx_buffer.length());
    if (0 < tx_count) {
      switch (ADAPTER_NUM) {
        case 0:
          if (Serial) {
            Serial.write(_tx_buffer.string(), tx_count);
            _tx_buffer.cull(tx_count);
            _adapter_set_flag(UART_FLAG_FLUSHED, (0 == _tx_buffer.length()));
          }
          break;
      }
    }
  }
  if (rxCapable()) {
    switch (ADAPTER_NUM) {
      case 0:
        if (Serial) {
          const uint8_t RX_BUF_LEN = 64;
          uint8_t ser_buffer[RX_BUF_LEN];
          uint8_t rx_len = 0;
          memset(ser_buffer, 0, RX_BUF_LEN);
          while ((RX_BUF_LEN > rx_len) && (0 < Serial.available())) {
            ser_buffer[rx_len++] = Serial.read();
          }
          if (rx_len > 0) {
            _rx_buffer.concat(ser_buffer, rx_len);
          }
        }
        break;
    }
    if (0 < _rx_buffer.length()) {
      if (nullptr != _read_cb_obj) {
        if (0 == _read_cb_obj->provideBuffer(&_rx_buffer)) {
          _rx_buffer.clear();
        }
      }
    }
  }
  return return_value;
}


int8_t UARTAdapter::_pf_init() {
  int8_t ret = -1;
  switch (ADAPTER_NUM) {
    case 0:
      Serial.begin(_opts.bitrate);   // USB
      _adapter_set_flag(UART_FLAG_HAS_TX | UART_FLAG_HAS_RX);
      _adapter_set_flag(UART_FLAG_UART_READY | UART_FLAG_FLUSHED);
      _adapter_clear_flag(UART_FLAG_PENDING_CONF | UART_FLAG_PENDING_RESET);
      ret = 0;
      break;
  }
  return ret;
}


int8_t UARTAdapter::_pf_deinit() {
  int8_t ret = -2;
  _adapter_clear_flag(UART_FLAG_UART_READY | UART_FLAG_PENDING_RESET | UART_FLAG_PENDING_CONF);
  if (ADAPTER_NUM < 3) {
    if (txCapable()) {
      //uart_wait_tx_idle_polling((uart_port_t) ADAPTER_NUM);
      _adapter_set_flag(UART_FLAG_FLUSHED);
    }
    ret = 0;
  }
  return ret;
}


/*
* Write to the UART.
*/
uint UARTAdapter::write(uint8_t* buf, uint len) {
  if (txCapable()) {
    _tx_buffer.concat(buf, len);
    _adapter_clear_flag(UART_FLAG_FLUSHED);
  }
  return len;  // TODO: StringBuilder needs an API enhancement to make this safe.
}


/*
* Write to the UART.
*/
uint UARTAdapter::write(char c) {
  if (txCapable()) {
    _tx_buffer.concat(c);
    _adapter_clear_flag(UART_FLAG_FLUSHED);
  }
  return 1;  // TODO: StringBuilder needs an API enhancement to make this safe.
}


/*
* Read from the class buffer.
*/
uint UARTAdapter::read(uint8_t* buf, uint len) {
  uint ret = strict_min((int32_t) len, (int32_t) _rx_buffer.length());
  if (0 < ret) {
    memcpy(buf, _rx_buffer.string(), ret);
    _rx_buffer.cull(ret);
  }
  return ret;
}


/*
* Read from the class buffer.
*/
uint UARTAdapter::read(StringBuilder* buf) {
  uint ret = _rx_buffer.length();
  if (0 < ret) {
    buf->concatHandoff(&_rx_buffer);
  }
  return ret;
}
