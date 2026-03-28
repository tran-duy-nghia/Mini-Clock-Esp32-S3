#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief DHT11 device type
 */
typedef struct dht11_t {
    void *rmt_handle;   // opaque pointer to RMT layer
} dht11_handle_t;

/**
 * @brief DHT11 data output
 */
typedef struct {
    uint8_t humidity;
    uint8_t temperature;
} dht11_data_t;

/**
 * @brief Initialize DHT11 device
 * @param dht11_handle_t DHT11 struct handle
 * @param gpio_num_t data gpio of DHT11
 * @return `ESP_OK` on success
 */
esp_err_t dht11_init(dht11_handle_t *dht_handle, gpio_num_t gpio_num);

/**
 * @brief Read DHT11 sensor
 * @param dht11_handle_t DHT11 struct handle
 * @param[out] dht11_data_t DHT11 data output
 * @return `ESP_OK` on success
 */
esp_err_t dht11_read(dht11_handle_t *dht_handle, dht11_data_t *out);

/**
 * @brief Deinitialize DHT11
 * @param dht11_handle_t dht11 struct handle
 * @return `ESP_OK` on success
 */
esp_err_t dht11_deinit(dht11_handle_t *dht_handle);

#ifdef __cplusplus
}
#endif