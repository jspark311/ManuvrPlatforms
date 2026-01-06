#include "../ESP32.h"
#include <BusQueue/UARTAdapter.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/queue.h"

#define UART_FIFO_CAPACITY   256

// Copied from "hal/uart_ll.h" because including that file causes the compile
//   to fail for some reason.
#define UART_INTR_RXFIFO_FULL       (0x1<<0)
#define UART_INTR_TXFIFO_EMPTY      (0x1<<1)
#define UART_INTR_PARITY_ERR        (0x1<<2)
#define UART_INTR_FRAM_ERR          (0x1<<3)
#define UART_INTR_RXFIFO_OVF        (0x1<<4)
#define UART_INTR_DSR_CHG           (0x1<<5)
#define UART_INTR_CTS_CHG           (0x1<<6)
#define UART_INTR_BRK_DET           (0x1<<7)
#define UART_INTR_RXFIFO_TOUT       (0x1<<8)
#define UART_INTR_SW_XON            (0x1<<9)
#define UART_INTR_SW_XOFF           (0x1<<10)
#define UART_INTR_GLITCH_DET        (0x1<<11)
#define UART_INTR_TX_BRK_DONE       (0x1<<12)
#define UART_INTR_TX_BRK_IDLE       (0x1<<13)
#define UART_INTR_TX_DONE           (0x1<<14)
#define UART_INTR_RS485_PARITY_ERR  (0x1<<15)
#define UART_INTR_RS485_FRM_ERR     (0x1<<16)
#define UART_INTR_RS485_CLASH       (0x1<<17)
#define UART_INTR_CMD_CHAR_DET      (0x1<<18)


static const char* TAG = "uart_drvr";
TaskHandle_t uart_polling_task_handle;
QueueHandle_t uart_queues[]  = {nullptr, nullptr, nullptr};
UARTAdapter* uart_instances[] = {nullptr, nullptr, nullptr};


static void IRAM_ATTR uart_event_task(void *pvParameters) {
  for (;;) {
    for (uint8_t port_idx = 0; port_idx < 3; port_idx++) {
      if (nullptr != uart_instances[port_idx]) {
        uart_instances[port_idx]->irq_handler();
      }
    }
    platform.yieldThread();
  }
  vTaskDelete(NULL);
}

#ifdef __cplusplus
}
#endif


/*
* In-class ISR handler. Be careful about state mutation....
*/
void PlatformUART::irq_handler() {
  uart_event_t event;
  bzero((void*) &event, sizeof(uart_event_t));
  // Waiting for UART event.
  if (nullptr == uart_queues[ADAPTER_NUM]) {  return;  }
  if (xQueueReceive(uart_queues[ADAPTER_NUM], (void *)&event, (TickType_t)10)) {
    //c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "UART IRQ");
    switch (event.type) {
      //Event of UART receving data
      case UART_DATA:
        {
          size_t dlen = 0;
          if (ESP_OK == uart_get_buffered_data_len((uart_port_t) ADAPTER_NUM, &dlen)) {
            if (dlen > 0) {
              last_byte_rx_time = millis();
              uint8_t dtmp[dlen + 4];
              bzero(dtmp, dlen + 4);
              int rlen = uart_read_bytes((uart_port_t) ADAPTER_NUM, dtmp, dlen, portMAX_DELAY);
              if (rlen > 0) {
                _rx_buffer.insert(dtmp, dlen);
              }
            }
          }
        }
        break;

      //Event of HW FIFO overflow detected
      case UART_FIFO_OVF:
        ESP_LOGI(TAG, "hw fifo overflow");
        // If fifo overflow happened, you should consider adding flow control for your application.
        // The ISR has already reset the rx FIFO,
        // As an example, we directly flush the rx buffer here in order to read more data.
        uart_flush_input((uart_port_t) ADAPTER_NUM);
        xQueueReset(uart_queues[ADAPTER_NUM]);
        break;
      //Event of UART ring buffer full
      case UART_BUFFER_FULL:
        ESP_LOGI(TAG, "ring buffer full");
        // If buffer full happened, you should consider encreasing your buffer size
        // As an example, we directly flush the rx buffer here in order to read more data.
        uart_flush_input((uart_port_t) ADAPTER_NUM);
        xQueueReset(uart_queues[ADAPTER_NUM]);
        break;
      //Event of UART RX break detected
      case UART_BREAK:
        ESP_LOGI(TAG, "uart rx break");
        break;
      //Event of UART parity check error
      case UART_PARITY_ERR:
        ESP_LOGI(TAG, "uart parity error");
        break;
      //Event of UART frame error
      case UART_FRAME_ERR:
        ESP_LOGI(TAG, "uart frame error");
        break;
      case UART_DATA_BREAK:
          ESP_LOGI(TAG, "uart data break");
          break;
      case UART_EVENT_MAX:
          ESP_LOGI(TAG, "uart event max");
          break;
      case UART_PATTERN_DET:
        //{
        //  uart_get_buffered_data_len((uart_port_t) port_idx, &buffered_size);
        //  int pos = uart_pattern_pop_pos((uart_port_t) port_idx);
        //  ESP_LOGI(TAG, "[UART PATTERN DETECTED] pos: %d, buffered size: %d", pos, buffered_size);
        //  if (pos == -1) {
        //      // There used to be a UART_PATTERN_DET event, but the pattern position queue is full so that it can not
        //      // record the position. We should set a larger queue size.
        //      // As an example, we directly flush the rx buffer here.
        //      uart_flush_input((uart_port_t) port_idx);
        //  } else {
        //      uart_read_bytes((uart_port_t) port_idx, dtmp, pos, 100 / portTICK_PERIOD_MS);
        //      uint8_t pat[PATTERN_CHR_NUM + 1];
        //      memset(pat, 0, sizeof(pat));
        //      uart_read_bytes((uart_port_t) port_idx, pat, PATTERN_CHR_NUM, 100 / portTICK_PERIOD_MS);
        //      ESP_LOGI(TAG, "read data: %s", dtmp);
        //      ESP_LOGI(TAG, "read pat : %s", pat);
        //  }
        //}
        break;
      //Others
      default:
        ESP_LOGI(TAG, "uart event type: %d", event.type);
        break;
    }
  }
}


/**
* Execute any I/O callbacks that are pending. The function is present because
*   this class contains the bus implementation.
*
* @return 0 on no action, 1 on successful action, -1 on error.
*/
int8_t PlatformUART::_pf_poll() {
  int8_t return_value = 0;
  if (txCapable() & (0 < _tx_buffer.count())) {
    // Refill the TX buffer...
    const uint32_t TX_COUNT = strict_min((uint32_t) (UART_FIFO_CAPACITY - 16), (uint32_t) _tx_buffer.count());
    if (0 < TX_COUNT) {
      uint8_t side_buffer[TX_COUNT] = {0, };
      const int32_t PEEK_COUNT = _tx_buffer.peek(side_buffer, TX_COUNT);
      const int32_t BYTES_WRITTEN = uart_write_bytes((uart_port_t) ADAPTER_NUM, (const char*) side_buffer, (size_t) PEEK_COUNT);
      if (BYTES_WRITTEN > 0) {
        _tx_buffer.cull(BYTES_WRITTEN);
        return_value |= 1;
      }
      uart_wait_tx_idle_polling((uart_port_t) ADAPTER_NUM);
    }
  }
  if (rxCapable()) {
    if (0 < _handle_rx_push()) {
      return_value |= 1;
    }
  }
  _flushed = _tx_buffer.isEmpty();
  return return_value;
}


int8_t PlatformUART::_pf_init() {
  int8_t ret = -1;
  if (ADAPTER_NUM < 3) {
    if (nullptr == uart_queues[ADAPTER_NUM]) {
      bool spawn_thread = !((nullptr != uart_queues[0]) || (nullptr != uart_queues[1]) || (nullptr != uart_queues[2]));
      uart_instances[ADAPTER_NUM] = this;
      const uint32_t rx_ring_size = (255 != _RXD_PIN) ? UART_FIFO_CAPACITY : 4;
      const uint32_t tx_ring_size = (255 != _TXD_PIN) ? UART_FIFO_CAPACITY : 4;
      if (ESP_OK == uart_driver_install((uart_port_t) ADAPTER_NUM, rx_ring_size, tx_ring_size, 10, &uart_queues[ADAPTER_NUM], 0)) {
        //ESP_LOGI(TAG, "uart_driver_install: %p", uart_queues[ADAPTER_NUM]);
        ret--;
        const uart_intr_config_t intr_conf {
          // UART_FRM_ERR_INT: Triggered when the receiver detects a data frame error .
          // UART_PARITY_ERR_INT: Triggered when the receiver detects a parity error in the data.
          // rx_timeout_thresh must be greater than rxfifo_full_thresh if UART_INTR_RXFIFO_TOUT is enabled.
          .intr_enable_mask = (UART_INTR_TXFIFO_EMPTY | UART_INTR_RXFIFO_FULL | UART_INTR_RXFIFO_TOUT | UART_INTR_RXFIFO_OVF | UART_INTR_TX_DONE),
          .rx_timeout_thresh        = 20,
          .txfifo_empty_intr_thresh = 16,
          .rxfifo_full_thresh       = 16
        };
        if (ESP_OK == uart_intr_config((uart_port_t) ADAPTER_NUM, &intr_conf)) {
          _adapter_set_flag(UART_FLAG_UART_READY);
          _adapter_clear_flag(UART_FLAG_PENDING_CONF | UART_FLAG_PENDING_RESET);
          _flushed = true;
          ret--;
          if (spawn_thread) {
            xTaskCreate(uart_event_task, "uart_task", 3000, NULL, 12, &uart_polling_task_handle);
          }
          if ((255 == _TXD_PIN) || (ESP_OK == uart_enable_tx_intr((uart_port_t) ADAPTER_NUM, 1, 16))) {
            ret--;
            if ((255 == _RXD_PIN) || (ESP_OK == uart_enable_rx_intr((uart_port_t) ADAPTER_NUM))) {
              ret = 0;
            }
          }
        }
      }
    }
    else {
      ret = 0;
    }
  }

  //
  if (0 == ret) {
    ret = -10;
    uart_word_length_t     databits;
    uart_parity_t          par_bits;
    uart_stop_bits_t       stp_bits;
    uart_hw_flowcontrol_t  flowctrl;

    switch (_opts.stop_bits) {
      default:
      case UARTStopBit::STOP_1:    stp_bits = UART_STOP_BITS_1;     break;
      case UARTStopBit::STOP_1_5:  stp_bits = UART_STOP_BITS_1_5;   break;
      case UARTStopBit::STOP_2:    stp_bits = UART_STOP_BITS_2;     break;
    }
    switch (_opts.parity) {
      default:
      case UARTParityBit::NONE:    par_bits = UART_PARITY_DISABLE;  break;
      case UARTParityBit::EVEN:    par_bits = UART_PARITY_EVEN;     break;
      case UARTParityBit::ODD:     par_bits = UART_PARITY_ODD;      break;
    }
    switch (_opts.flow_control) {
      default:                           // These are software ideas. Hardware sees no flowcontrol.
      case UARTFlowControl::XON_XOFF_R:  // These are software ideas. Hardware sees no flowcontrol.
      case UARTFlowControl::XON_XOFF_T:  // These are software ideas. Hardware sees no flowcontrol.
      case UARTFlowControl::XON_XOFF_RT: // These are software ideas. Hardware sees no flowcontrol.
      case UARTFlowControl::NONE:         flowctrl = UART_HW_FLOWCTRL_DISABLE;  break;
      case UARTFlowControl::RTS:          flowctrl = UART_HW_FLOWCTRL_RTS;      break;
      case UARTFlowControl::CTS:          flowctrl = UART_HW_FLOWCTRL_CTS;      break;
      case UARTFlowControl::RTS_CTS:      flowctrl = UART_HW_FLOWCTRL_CTS_RTS;  break;
    }
    switch (_opts.bit_per_word) {
      default:
      case 8:  databits = UART_DATA_8_BITS;  break;
      case 7:  databits = UART_DATA_7_BITS;  break;
      case 6:  databits = UART_DATA_6_BITS;  break;
      case 5:  databits = UART_DATA_5_BITS;  break;
    }
    uart_config_t uart_config = {
      .baud_rate = (int) _opts.bitrate,
      .data_bits = databits,
      .parity = par_bits,
      .stop_bits = stp_bits,
      .flow_ctrl = flowctrl,
      .rx_flow_ctrl_thresh = 122,  // The FIFO is 128 bytes (I think?)
      .source_clk = UART_SCLK_DEFAULT
    };
    if (ESP_OK == uart_param_config((uart_port_t) ADAPTER_NUM, &uart_config)) {
      int rxpin  = (255 == _RXD_PIN) ? -1 : _RXD_PIN;
      int txpin  = (255 == _TXD_PIN) ? -1 : _TXD_PIN;
      int rtspin = (255 == _RTS_PIN) ? -1 : _RTS_PIN;
      int ctspin = (255 == _CTS_PIN) ? -1 : _CTS_PIN;
      ret--;
      if (ESP_OK == uart_set_pin((uart_port_t) ADAPTER_NUM, txpin, rxpin, rtspin, ctspin)) {
        _adapter_set_flag(UART_FLAG_HAS_TX, (-1 != txpin));
        _adapter_set_flag(UART_FLAG_HAS_RX, (-1 != rxpin));
        uart_flush_input((uart_port_t) ADAPTER_NUM);
        //xQueueReset(uart_queues[ADAPTER_NUM]);
        ret = 0;
      }
    }
  }
  return ret;
}


int8_t PlatformUART::_pf_deinit() {
  int8_t ret = -2;
  _adapter_clear_flag(UART_FLAG_UART_READY | UART_FLAG_PENDING_RESET | UART_FLAG_PENDING_CONF);
  uart_disable_tx_intr((uart_port_t) ADAPTER_NUM);
  uart_disable_rx_intr((uart_port_t) ADAPTER_NUM);
  if (ADAPTER_NUM < 3) {
    if (txCapable()) {
      uart_wait_tx_idle_polling((uart_port_t) ADAPTER_NUM);
      _flushed = true;
    }
    if (ESP_OK == uart_driver_delete((uart_port_t) ADAPTER_NUM)) {
    }
    ret = 0;
  }
  return ret;
}
