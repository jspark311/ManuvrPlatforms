/*
File:   SPIAdapter.cpp
Author: J. Ian Lindsay
Date:   2016.12.17

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

*/
#include "../ESP32.h"
#include <BusQueue/SPIAdapter.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/spi_master.h"


/*******************************************************************************
* .-. .----..----.    .-.     .--.  .-. .-..----.
* | |{ {__  | {}  }   | |    / {} \ |  `| || {}  \
* | |.-._} }| .-. \   | `--./  /\  \| |\  ||     /
* `-'`----' `-' `-'   `----'`-'  `-'`-' `-'`----'
*
* Interrupt service routine support functions. Everything in this block
*   executes under an ISR. Keep it brief...
*******************************************************************************/

//static lldesc_t _ll_tx;
static SPIBusOp* _threaded_op[2]  = {nullptr, nullptr};
spi_device_handle_t spi_handle[2];
TaskHandle_t static_spi_thread_id = 0;


static void* IRAM_ATTR spi_worker_thread(void* arg) {
  SPIAdapter* BUSPTR = (SPIAdapter*) arg;
  uint8_t anum = BUSPTR->adapterNumber();
  while (1) {
    if (nullptr != _threaded_op[anum]) {
      SPIBusOp* op = _threaded_op[anum];
      op->advance_operation(0, 0);
			_threaded_op[anum] = nullptr;
      platform.yieldThread();
    }
    else if (0 == BUSPTR->poll()) {
      //platform.suspendThread();
      platform.yieldThread();
    }
  }
  return nullptr;
}

#ifdef __cplusplus
}
#endif




/*******************************************************************************
* ___     _                                  This is a template class for
*  |   / / \ o    /\   _|  _. ._ _|_  _  ._  defining arbitrary I/O adapters.
* _|_ /  \_/ o   /--\ (_| (_| |_) |_ (/_ |   Adapters must be instanced with
*                             |              a BusOp as the template param.
*******************************************************************************/

int8_t SPIAdapter::bus_init() {
	spi_bus_config_t bus_config;
	uint32_t b_flags = 0;
	spi_host_device_t host_id;
	uint8_t dma_chan;
	uint8_t anum = adapterNumber();
	switch (anum) {
		case 0:
			host_id = HSPI_HOST;
			dma_chan = 2;
			break;
		case 1:
			host_id = VSPI_HOST;
			dma_chan = 1;
			break;
		default:
			return -2;
	}

  if (255 != _CLK_PIN) {
    b_flags |= SPICOMMON_BUSFLAG_SCLK;
    bus_config.sclk_io_num = (gpio_num_t) _CLK_PIN;
  }
  else {
    bus_config.sclk_io_num = GPIO_NUM_NC;
  }
  if (255 != _MOSI_PIN) {
    b_flags |= SPICOMMON_BUSFLAG_MOSI;
    bus_config.mosi_io_num = (gpio_num_t) _MOSI_PIN;
  }
  else {
    bus_config.mosi_io_num = GPIO_NUM_NC;
  }
  if (255 != _MISO_PIN) {
    b_flags |= SPICOMMON_BUSFLAG_MISO;
    bus_config.miso_io_num = (gpio_num_t) _MISO_PIN;
  }
  else {
    bus_config.miso_io_num = GPIO_NUM_NC;
  }
	bus_config.quadwp_io_num   = GPIO_NUM_NC;    // Not used
	bus_config.quadhd_io_num   = GPIO_NUM_NC;    // Not used
	bus_config.max_transfer_sz = 65535;   // 0 means use default.
  bus_config.flags           = b_flags;
  bus_config.intr_flags      = ESP_INTR_FLAG_IRAM;

	esp_err_t errRc = spi_bus_initialize(
		host_id,
		&bus_config,
		dma_chan
	);

	if (errRc != ESP_OK) {
		c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "SPI%u: spi_bus_initialize(): rc=%d", ADAPTER_NUM, errRc);
    return -1;
	}

	spi_device_interface_config_t dev_config;
	dev_config.address_bits     = 0;
	dev_config.command_bits     = 0;
	dev_config.dummy_bits       = 0;
	dev_config.mode             = 0;
	dev_config.duty_cycle_pos   = 0;
	dev_config.cs_ena_posttrans = 0;
	dev_config.cs_ena_pretrans  = 0;
	dev_config.clock_speed_hz   = 9000000;
	dev_config.spics_io_num     = (gpio_num_t) -1;
	dev_config.flags            = SPI_DEVICE_NO_DUMMY;
	dev_config.queue_size       = 1;
	dev_config.pre_cb           = nullptr;
	dev_config.post_cb          = nullptr;
	c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "SPI%u: Adding device bus.", ADAPTER_NUM);
	errRc = spi_bus_add_device(host_id, &dev_config, &spi_handle[anum]);
	if (errRc != ESP_OK) {
    c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "SPI%u: spi_bus_add_device(): rc=%d", ADAPTER_NUM, errRc);
  	return -2;
	}

  unsigned long _thread_id = 0;
  PlatformThreadOpts topts;
  topts.thread_name = "SPI";
  topts.stack_sz    = 2048;
  topts.priority    = 0;
  topts.core        = 1;   // TODO: Is this the best choice? Might use a preprocessor define.

	platform.createThread(&_thread_id, nullptr, spi_worker_thread, (void*) this, &topts);
  static_spi_thread_id = (TaskHandle_t) _thread_id;
  c3p_log(LOG_LEV_INFO, __PRETTY_FUNCTION__, "Spawned SPI thread: %lu", _thread_id);
  _adapter_set_flag(SPI_FLAG_SPI_READY);
  return 0;
}


int8_t SPIAdapter::bus_deinit() {
  _adapter_clear_flag(SPI_FLAG_SPI_READY);
  return 0;
}



/*******************************************************************************
* BusOp functions below...
*******************************************************************************/

/**
* Calling this member will cause the bus operation to be started.
*
* @return 0 on success, or non-zero on failure.
*/
XferFault SPIBusOp::begin() {
	XferFault ret = XferFault::NO_REASON;
	uint8_t anum = _bus->adapterNumber();
  //time_began    = micros();
  switch (anum) {
    case 0:
    case 1:
			if (nullptr == _threaded_op[anum]) {
        if ((nullptr == callback) || (0 == callback->io_op_callahead(this))) {
          _threaded_op[anum] = this;
          if (0 != static_spi_thread_id) {
            vTaskResume(static_spi_thread_id);
          }
          ret = XferFault::NONE;
        }
        else {
          ret = XferFault::IO_RECALL;
          abort(ret);
        }
			}
			else {
        ret = XferFault::BUS_BUSY;
			}
      break;
    case 2:   // SPI workflow
    default:
      ret = XferFault::BUS_FAULT;
      abort(ret);
      break;
  }
  return ret;
}


/**
* Called from the ISR to advance this operation on the bus.
* Stay brief. We are in an ISR.
*
* @return 0 on success. Non-zero on failure.
*/
int8_t SPIBusOp::advance_operation(uint32_t status_reg, uint8_t data_reg) {
	static spi_transaction_t txns[2];
	XferFault ret = XferFault::BUS_FAULT;
  bool run_xfer = false;
	set_state(XferState::INITIATE);  // Indicate that we now have bus control.
	uint8_t anum = _bus->adapterNumber();
	memset(&txns[anum], 0, sizeof(spi_transaction_t));
  txns[anum].flags |= SPI_DEVICE_HALFDUPLEX;

  if (_param_len) {
    set_state(XferState::ADDR);
    memcpy(&(txns[anum].tx_data), xfer_params, transferParamLength());
		txns[anum].flags |= SPI_TRANS_USE_TXDATA;
    txns[anum].length = _param_len * 8;
    run_xfer = true;
  }

  if (_buf_len) {
    switch (get_opcode()) {
      case BusOpcode::TX:
        set_state(XferState::TX_WAIT);
				txns[anum].tx_buffer = _buf;
				txns[anum].length = _buf_len * 8;
        run_xfer = true;
        break;
      case BusOpcode::RX:
        set_state(XferState::RX_WAIT);
				txns[anum].rx_buffer = _buf;
				txns[anum].rxlength = _buf_len * 8;
        run_xfer = true;
        break;
      default:
        ret = XferFault::BAD_PARAM;
        break;
    }
  }

  if (run_xfer) {
    _assert_cs(true);
    if (ESP_OK == spi_device_queue_trans(spi_handle[anum], &txns[anum], portMAX_DELAY)) {
      spi_transaction_t* pending_txn;
      if (ESP_OK == spi_device_get_trans_result(spi_handle[anum], &pending_txn, portMAX_DELAY)) {
        ret = XferFault::NONE;
      }
      else {
        // portMAX_DELAY used in the calls above will cause the op to never timeout.
        c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "spi_device_get_trans_result() gave us a bad outcome.");
      }
    }
    else {
      c3p_log(LOG_LEV_ERROR, __PRETTY_FUNCTION__, "spi_device_queue_trans() gave us a bad outcome.");
    }
  }

  if (XferFault::NONE != ret) {
    abort(ret);
  }
  else {
    markComplete();
  }
  return 0;
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
