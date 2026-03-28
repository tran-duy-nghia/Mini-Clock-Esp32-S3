#include "dht_onewire_rmt_interface.h"
#include "esp_check.h"

#define DHT_RMT_RESOLUTION_HZ 1000000
#define DHT_RMT_DEFAULT_MEM_BLOCK_SYMBOLS 48
#define DHT_RMT_RX_MEM_BLOCK_SIZE DHT_RMT_DEFAULT_MEM_BLOCK_SYMBOLS

#define MASTER_START_BIT_SAMPLE_TIME 15000
#define DHT_RESPONSE_BIT_SAMPLE_TIME 80
#define DHT_BIT_HIGHT_SAMPLE_TIME 60

#define DHT_RMT_SYMBOLS_MAX 64
#define DHT_RMT_MAX_RECEIVE_BYTE 5 /*!< 2bytes Humi + 2bytes Temp + 1byte Crc*/
#define DHT_RMT_MAX_RECEIVE_BIT 40

#define DHT_EXPECTED_RX_SYMBOL_NUM 43 /*!< 1bit start + 2bits data + 40bits response */

static const char *TAG = "1-wire.rmt";

const static rmt_receive_config_t rmt_rx_config = {
    .signal_range_min_ns = 1000,     // 1µs filter
    .signal_range_max_ns = 30000000, // 30ms → frame end
};

static esp_err_t dht_onewire_rmt_start(dht_onewire_rmt_handle_t bus_handle);
static esp_err_t dht_onewire_rmt_decode(rmt_symbol_word_t *symbols, size_t symbol_num, uint8_t *rx_buf, size_t rx_buf_size);

IRAM_ATTR
bool onewire_rmt_rx_done_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_data) {
    BaseType_t task_woken = pdFALSE;
    dht_onewire_rmt_handle_t *bus_rmt = (dht_onewire_rmt_handle_t *)user_data;

    xQueueSendFromISR(bus_rmt->rx_status_queue, edata, &task_woken);

    return task_woken;
}

esp_err_t dht_onewire_rmt_init(dht_onewire_rmt_handle_t *bus_handle) {
    esp_err_t ret = ESP_OK;

    // Create RMT Rx channel
    rmt_rx_channel_config_t rx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = DHT_RMT_RESOLUTION_HZ,
        .gpio_num = bus_handle->data_gpio,
        .mem_block_symbols = DHT_RMT_DEFAULT_MEM_BLOCK_SYMBOLS,
    };
    ESP_GOTO_ON_ERROR(rmt_new_rx_channel(&rx_chan_config, &bus_handle->rx_channel), err, TAG, "create rmt rx channel failed");

    rmt_rx_event_callbacks_t cbs = {
        .on_recv_done = onewire_rmt_rx_done_callback,
    };
    ESP_GOTO_ON_ERROR(rmt_rx_register_event_callbacks(bus_handle->rx_channel, &cbs, bus_handle), err, TAG, "enable rmt rx channel failed");
    ESP_GOTO_ON_ERROR(rmt_enable(bus_handle->rx_channel), err, TAG, "enable rmt rx channel failed");

    // Set Data gpio to Open drain mode
    gpio_od_enable(bus_handle->data_gpio);
    gpio_set_pull_mode(bus_handle->data_gpio, GPIO_FLOATING); // Set to pullup if hardware pullup not supported

    // DHT Transfer 40bit per transition, each RMT symbol represent 1bit,
    // but buffer should be bigger than 40 because of some extra symbol when DHT response
    bus_handle->rx_symbol_buf = malloc(sizeof(rmt_symbol_word_t) * DHT_RMT_SYMBOLS_MAX);
    ESP_GOTO_ON_FALSE(bus_handle->rx_symbol_buf, ESP_ERR_NO_MEM, err, TAG, "no mem to store RMT receive symbol buffer");

    bus_handle->rx_status_queue = xQueueCreate(1, sizeof(rmt_rx_done_event_data_t));
    ESP_GOTO_ON_FALSE(bus_handle->rx_status_queue, ESP_ERR_NO_MEM, err, TAG, "rx status queue creation failed");

    bus_handle->mutex = xSemaphoreCreateMutex();
    ESP_GOTO_ON_FALSE(bus_handle->mutex, ESP_ERR_NO_MEM, err, TAG, "bus mutex creation failed");

    return ret;
err:
    dht_onewire_del(bus_handle);
    return ret;
}

esp_err_t dht_onewire_del(dht_onewire_rmt_handle_t *bus_handle) {
    if (bus_handle->rx_channel) {
        rmt_disable(bus_handle->rx_channel);
        rmt_del_channel(bus_handle->rx_channel);
    }
    if (bus_handle->rx_status_queue) {
        vQueueDelete(bus_handle->rx_status_queue);
    }
    if (bus_handle->mutex) {
        vSemaphoreDelete(bus_handle->mutex);
    }
    if (bus_handle->rx_symbol_buf) {
        free(bus_handle->rx_symbol_buf);
    }
    if (bus_handle->data_gpio != GPIO_NUM_NC)
        gpio_od_disable(bus_handle->data_gpio);
    return ESP_OK;
}

esp_err_t dht_onewire_read(dht_onewire_rmt_handle_t bus_handle, uint8_t *rx_buf, size_t rx_buf_size) {
    esp_err_t ret = ESP_OK;
    const int rmt_word_rx_buf_size = sizeof(rmt_symbol_word_t) * DHT_RMT_SYMBOLS_MAX;

    ESP_RETURN_ON_FALSE(rx_buf_size <= DHT_RMT_MAX_RECEIVE_BYTE, ESP_ERR_INVALID_ARG, TAG, "rx_buf_size too large for buffer to hold");
    memset(rx_buf, 0, rx_buf_size);

    xSemaphoreTake(bus_handle.mutex, portMAX_DELAY);
    ESP_GOTO_ON_ERROR(rmt_receive(bus_handle.rx_channel, bus_handle.rx_symbol_buf, rmt_word_rx_buf_size, &rmt_rx_config), err, TAG,
                      "rmt receive failed");
    dht_onewire_rmt_start(bus_handle);

    rmt_rx_done_event_data_t edata;
    ESP_GOTO_ON_ERROR(xQueueReceive(bus_handle.rx_status_queue, &edata, pdMS_TO_TICKS(1000)) != pdTRUE, err, TAG, "receive timeoout");

    ret = dht_onewire_rmt_decode(bus_handle.rx_symbol_buf, edata.num_symbols, rx_buf, rx_buf_size);

err:
    xSemaphoreGive(bus_handle.mutex);
    return ret;
}

esp_err_t dht_onewire_rmt_start(dht_onewire_rmt_handle_t bus_handle) {
    gpio_set_direction(bus_handle.data_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(bus_handle.data_gpio, 0);
    esp_rom_delay_us(20000); // MCU pull low 20us for start signal

    gpio_set_level(bus_handle.data_gpio, 1);
    esp_rom_delay_us(30); // MCU release bus, wait for response

    gpio_set_direction(bus_handle.data_gpio, GPIO_MODE_INPUT);

    return ESP_OK;
}

esp_err_t dht_onewire_rmt_decode(rmt_symbol_word_t *symbols, size_t symbol_num, uint8_t *rx_buf, size_t rx_buf_size) {
    if (!symbols || symbol_num < DHT_EXPECTED_RX_SYMBOL_NUM) {
        return ESP_ERR_INVALID_SIZE;
    }
    uint8_t data[5] = {0};
    int symbol_index = 0;
    uint16_t bitcount = 0;

    while (symbol_index < symbol_num && bitcount < DHT_RMT_MAX_RECEIVE_BIT) {
        // After bus initialize, logic level is high, so after any start condition, 
        // duration 0 is always for pulse LOW, duration 1 is always for pulse HIGH
        uint16_t duration_low = symbols[symbol_index].duration0;
        uint16_t duration_high = symbols[symbol_index].duration1;
        if (duration_low >= MASTER_START_BIT_SAMPLE_TIME ||
            (duration_low >= DHT_RESPONSE_BIT_SAMPLE_TIME || duration_high >= DHT_RESPONSE_BIT_SAMPLE_TIME)) {
            // Skip these symbols, decode data symbols only
            symbol_index++;
            continue;
        }
        data[bitcount / 8] <<= 1;
        if (duration_high > DHT_BIT_HIGHT_SAMPLE_TIME)
            data[bitcount / 8] |= 1;
        bitcount++;
        symbol_index++;
    }
    if (bitcount != 40) {
        return ESP_ERR_INVALID_SIZE;
    }

    if ((data[0] + data[1] + data[2] + data[3]) != data[4]) {
        return ESP_ERR_INVALID_CRC;
    }

    memcpy(rx_buf, data, rx_buf_size);

    return ESP_OK;
}