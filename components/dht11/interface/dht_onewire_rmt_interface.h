#pragma once

#include <stdint.h>
#include "driver/gpio.h"
#include "driver/rmt_rx.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief DHT 1-Wire bus struct handle
 */
typedef struct {
    rmt_channel_handle_t rx_channel;
    gpio_num_t data_gpio;
    rmt_symbol_word_t *rx_symbol_buf;
    QueueHandle_t rx_status_queue;  /* !< This queue use for notify bus receive status only, high layer api should not call this*/
    SemaphoreHandle_t mutex;

} dht_onewire_rmt_handle_t;

/**
 * @brief DHT 1-Wire bus initialization
 * @param bus_handle 1-wire bus handle
 * @return `ESP_OK` on success
 */
esp_err_t dht_onewire_rmt_init(dht_onewire_rmt_handle_t *bus_handle);

/**
 * @brief DHT 1-Wire bus deletion
 * @param bus_handle 1-wire bus handle
 * @return `ESP_OK` on success
 */
esp_err_t dht_onewire_del(dht_onewire_rmt_handle_t *bus_handle);

/**
 * @brief Read out data from DHT sensor on 1-Wire bus
 * @param bus_handle 1-wire bus handle
 * @param[out] rx_buf raw data buffer
 * @param rx_buf_size raw data buffer size, should be 5 (standard DHT output package size)
 * @return `ESP_OK` on success
 */
esp_err_t dht_onewire_read(dht_onewire_rmt_handle_t bus_handle, uint8_t *rx_buf, size_t rx_buf_size);

#ifdef __cplusplus
}
#endif