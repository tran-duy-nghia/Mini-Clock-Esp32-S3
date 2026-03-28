#include "dht11.h"
#include "dht_onewire_rmt_interface.h"
#include "esp_check.h"

static const char *TAG = "dht11";

esp_err_t dht11_init(dht11_handle_t *dht_handle, gpio_num_t gpio_num) {
    ESP_RETURN_ON_FALSE(dht_handle, ESP_ERR_INVALID_ARG, TAG, "dev is null");

    // allocate RMT handle
    dht_onewire_rmt_handle_t *rmt = calloc(1, sizeof(dht_onewire_rmt_handle_t));
    ESP_RETURN_ON_FALSE(rmt, ESP_ERR_NO_MEM, TAG, "no memory for rmt handle");

    rmt->data_gpio = gpio_num;

    esp_err_t ret = dht_onewire_rmt_init(rmt);
    if (ret != ESP_OK) {
        free(rmt);
        return ret;
    }

    dht_handle->rmt_handle = rmt;

    ESP_LOGI(TAG, "DHT11 initialized on GPIO %d", gpio_num);

    return ESP_OK;
}

esp_err_t dht11_read(dht11_handle_t *dht_handle, dht11_data_t *out) {
    ESP_RETURN_ON_FALSE(dht_handle && out, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    dht_onewire_rmt_handle_t *rmt = (dht_onewire_rmt_handle_t *)dht_handle->rmt_handle;
    ESP_RETURN_ON_FALSE(rmt, ESP_ERR_INVALID_STATE, TAG, "rmt not initialized");

    uint8_t raw[5] = {0};

    esp_err_t ret = dht_onewire_read(*rmt, raw, sizeof(raw));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "read failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Parse result
    out->humidity = raw[0];
    out->temperature = raw[2];

    return ESP_OK;
}

esp_err_t dht11_deinit(dht11_handle_t *dht_handle) {
    ESP_RETURN_ON_FALSE(dht_handle, ESP_ERR_INVALID_ARG, TAG, "dev is null");

    dht_onewire_rmt_handle_t *rmt = (dht_onewire_rmt_handle_t *)dht_handle->rmt_handle;

    if (rmt) {
        dht_onewire_del(rmt);
        free(rmt);
        dht_handle->rmt_handle = NULL;
    }

    ESP_LOGI(TAG, "DHT11 deinitialized");

    return ESP_OK;
}
