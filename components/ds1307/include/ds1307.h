#pragma once

#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DS1307_ADDRESS 0x68

/**
 * Squarewave frequency
 */
typedef enum
{
    DS1307_1HZ = 0, //!< 1 Hz
    DS1307_4096HZ,  //!< 4096 Hz
    DS1307_8192HZ,  //!< 8192 Hz
    DS1307_32768HZ  //!< 32768 Hz
} ds1307_squarewave_freq_t;

typedef struct {
    i2c_master_dev_handle_t dev;
    SemaphoreHandle_t mutex;

} ds1307_handle_t;

/**
 * @brief Initialize ds1307_t, register ds1307 device to i2c bus
 *
 * @param ds1307_handle_t ds1307 structure
 * @param bus_handle i2c_master_bus_handle_t 
 * @return `ESP_OK` on success
 */
esp_err_t ds1307_init(ds1307_handle_t *ds1307, i2c_master_bus_handle_t bus_handle);

/**
 * @brief Free ds1307 device from i2c bus
 *
 * @param ds1307_handle_t ds1307 structure
 * @return `ESP_OK` on success
 */
esp_err_t ds1307_free(ds1307_handle_t *ds1307);

/**
 * @brief Start/stop clock
 *
 * @param ds1307_handle_t ds1307 structure
 * @param start Start clock if true
 * @return `ESP_OK` on success
 */
esp_err_t ds1307_start(ds1307_handle_t *ds1307, bool start);

/**
 * @brief Get current clock state
 *
 * @param ds1307_handle_t ds1307 structure
 * @param[out] running true if clock running
 * @return `ESP_OK` on success
 */
esp_err_t ds1307_is_running(ds1307_handle_t *ds1307, bool *running);

/**
 * @brief Get current time
 *
 * @param ds1307_handle_t ds1307 structure
 * @param[out] time Pointer to the time struct to fill
 * @return `ESP_OK` on success
 */
esp_err_t ds1307_get_time(ds1307_handle_t *ds1307, struct tm *time);

/**
 * @brief Set time to RTC
 *
 * @param ds1307_handle_t ds1307 structure
 * @param[in] time Pointer to the time struct
 * @return `ESP_OK` on success
 */
esp_err_t ds1307_set_time(ds1307_handle_t *ds1307, const struct tm *time);

/**
 * @brief Enable or disable square-wave oscillator output
 *
 * @param ds1307_handle_t ds1307 structure
 * @param enable Enable oscillator if true
 * @return `ESP_OK` on success
 */
esp_err_t ds1307_enable_squarewave(ds1307_handle_t *ds1307, bool enable);

/**
 * @brief Get square-wave oscillator output
 *
 * @param ds1307_handle_t ds1307 structure
 * @param[out] sqw_en true if square-wave oscillator enabled
 * @return `ESP_OK` on success
 */
esp_err_t ds1307_is_squarewave_enabled(ds1307_handle_t *ds1307, bool *sqw_en);

/**
 * @brief Set square-wave oscillator frequency
 *
 * @param ds1307_handle_t ds1307 structure
 * @param freq Frequency
 * @return `ESP_OK` on success
 */
esp_err_t ds1307_set_squarewave_freq(ds1307_handle_t *ds1307, ds1307_squarewave_freq_t freq);

/**
 * @brief Get current square-wave oscillator frequency
 *
 * @param ds1307_handle_t ds1307 structure
 * @param[out] sqw_freq Frequency
 * @return `ESP_OK` on success
 */
esp_err_t ds1307_get_squarewave_freq(ds1307_handle_t *ds1307, ds1307_squarewave_freq_t *sqw_freq);

/**
 * @brief Set output level of the SQW/OUT pin
 *
 * Set output level if square-wave output is disabled
 *
 * @param ds1307_handle_t ds1307 structure
 * @param value High level if true
 * @return `ESP_OK` on success
 */
esp_err_t ds1307_set_output(ds1307_handle_t *ds1307, bool value);

/**
 * @brief Get current output level of the SQW/OUT pin
 *
 * @param ds1307_handle_t ds1307 structure
 * @param[out] out current output level of the SQW/OUT pin, true if high
 * @return `ESP_OK` on success
 */
esp_err_t ds1307_get_output(ds1307_handle_t *ds1307, bool *out);

/**
 * @brief Write buffer to RTC RAM
 *
 * @param ds1307_handle_t ds1307 structure
 * @param offset Start byte, 0..55
 * @param buf Buffer
 * @param len Bytes to write, 1..56
 * @return `ESP_OK` on success
 */
esp_err_t ds1307_write_ram(ds1307_handle_t *ds1307, uint8_t offset, uint8_t *buf, uint8_t len);

/**
 * @brief Read RAM contents into the buffer
 *
 * @param ds1307_handle_t ds1307 structure
 * @param offset Start byte, 0..55
 * @param[out] buf Buffer to store data
 * @param len Bytes to read, 1..56
 * @return `ESP_OK` on success
 */
esp_err_t ds1307_read_ram(ds1307_handle_t *ds1307, uint8_t offset, uint8_t *buf, uint8_t len);

#ifdef __cplusplus
}
#endif
