#include "ds1307.h"
#include "esp_log.h"

#define I2C_FREQ_HZ 400000
#define I2C_TIMEOUT_MS 100

#define RAM_SIZE 56

#define TIME_REG 0
#define CONTROL_REG 7
#define RAM_REG 8

#define CH_BIT (1 << 7)
#define HOUR12_BIT (1 << 6)
#define PM_BIT (1 << 5)
#define SQWE_BIT (1 << 4)
#define OUT_BIT (1 << 7)

#define CH_MASK 0x7f
#define SECONDS_MASK 0x7f
#define HOUR12_MASK 0x1f
#define HOUR24_MASK 0x3f
#define SQWEF_MASK 0xfc
#define SQWE_MASK 0xef
#define OUT_MASK 0x7f

#define CHECK_ARG(ARG)                                                                                                                     \
    do {                                                                                                                                   \
        if (!(ARG)) {                                                                                                                      \
            ESP_LOGE("DS1307", "Invalid argument: %s", #ARG);                                                                              \
            return ESP_ERR_INVALID_ARG;                                                                                                    \
        }                                                                                                                                  \
    } while (0)

#define DS1307_LOCK(mutex)                                                                                                                 \
    do {                                                                                                                                   \
        if (xSemaphoreTake((mutex), pdMS_TO_TICKS(100)) != pdTRUE) {                                                                       \
            return ESP_ERR_TIMEOUT;                                                                                                        \
        }                                                                                                                                  \
    } while (0)

#define DS1307_UNLOCK(mutex) xSemaphoreGive((mutex))

static uint8_t bcd2dec(uint8_t val) { return (val >> 4) * 10 + (val & 0x0f); }

static uint8_t dec2bcd(uint8_t val) { return ((val / 10) << 4) + (val % 10); }

// Static api for i2c transition, caller must hold mutex
static esp_err_t get_register(ds1307_handle_t *ds1307, uint8_t reg, uint8_t *val, size_t read_size) {
    return i2c_master_transmit_receive(ds1307->dev, &reg, 1, val, read_size, I2C_TIMEOUT_MS);
}

static esp_err_t set_register(ds1307_handle_t *ds1307, uint8_t reg, const uint8_t *val, size_t write_size) {
    esp_err_t err;
    uint8_t *buf = malloc(write_size + 1);
    if (!buf)
        return ESP_ERR_NO_MEM;
    buf[0] = reg;
    memcpy(buf + 1, val, write_size);
    err = i2c_master_transmit(ds1307->dev, buf, write_size + 1, I2C_TIMEOUT_MS);
    free(buf);
    return err;
}

static esp_err_t update_register(ds1307_handle_t *ds1307, uint8_t reg, uint8_t mask, uint8_t val) {
    esp_err_t err;
    uint8_t old;

    err = get_register(ds1307, reg, &old, 1);
    if (err != ESP_OK) {
        return err;
    }
    uint8_t buf = (old & mask) | val;
    return set_register(ds1307, reg, &buf, 1);
}

esp_err_t ds1307_init(ds1307_handle_t *ds1307, i2c_master_bus_handle_t bus_handle) {
    CHECK_ARG(ds1307 && bus_handle);

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = DS1307_ADDRESS,
        .scl_speed_hz = I2C_FREQ_HZ,
    };
    esp_err_t err = i2c_master_bus_add_device(bus_handle, &dev_config, &ds1307->dev);
    if (err != ESP_OK) {
        return err;
    }

    ds1307->mutex = xSemaphoreCreateMutex();
    if (ds1307->mutex == NULL) {
        i2c_master_bus_rm_device(ds1307->dev);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t ds1307_free(ds1307_handle_t *ds1307) {
    CHECK_ARG(ds1307);

    if (ds1307->dev) {
        i2c_master_bus_rm_device(ds1307->dev);
    }

    if (ds1307->mutex) {
        vSemaphoreDelete(ds1307->mutex);
    }

    return ESP_OK;
}

esp_err_t ds1307_start(ds1307_handle_t *ds1307, bool start) {
    CHECK_ARG(ds1307);

    esp_err_t err;
    DS1307_LOCK(ds1307->mutex);
    err = update_register(ds1307, TIME_REG, CH_MASK, start ? 0 : CH_BIT);
    DS1307_UNLOCK(ds1307->mutex);
    if (err != ESP_OK) {
        return err;
    }
    return ESP_OK;
}

esp_err_t ds1307_is_running(ds1307_handle_t *ds1307, bool *running) {
    CHECK_ARG(ds1307 && running);

    esp_err_t err;
    uint8_t val;

    DS1307_LOCK(ds1307->mutex);
    err = get_register(ds1307, TIME_REG, &val, 1);
    DS1307_UNLOCK(ds1307->mutex);
    if (err != ESP_OK) {
        return err;
    }
    *running = val & CH_BIT ? false : true;

    return ESP_OK;
}

esp_err_t ds1307_get_time(ds1307_handle_t *ds1307, struct tm *time) {
    CHECK_ARG(ds1307 && time);

    esp_err_t err;
    uint8_t buf[7];

    DS1307_LOCK(ds1307->mutex);
    err = get_register(ds1307, TIME_REG, buf, 7);
    DS1307_UNLOCK(ds1307->mutex);
    if (err != ESP_OK) {
        return err;
    }

    time->tm_sec = bcd2dec(buf[0] & SECONDS_MASK);
    time->tm_min = bcd2dec(buf[1]);
    if (buf[2] & HOUR12_BIT) {
        // RTC in 12-hour mode
        time->tm_hour = bcd2dec(buf[2] & HOUR12_MASK) - 1;
        if (buf[2] & PM_BIT)
            time->tm_hour += 12;
    } else
        time->tm_hour = bcd2dec(buf[2] & HOUR24_MASK);
    time->tm_wday = bcd2dec(buf[3]) - 1;
    time->tm_mday = bcd2dec(buf[4]);
    time->tm_mon = bcd2dec(buf[5]) - 1;
    time->tm_year = bcd2dec(buf[6]) + 100;
    return ESP_OK;
}

esp_err_t ds1307_set_time(ds1307_handle_t *ds1307, const struct tm *time) {
    CHECK_ARG(ds1307 && time);

    esp_err_t err;
    uint8_t buf[7] = {dec2bcd(time->tm_sec) & ~CH_BIT, dec2bcd(time->tm_min),     dec2bcd(time->tm_hour),      dec2bcd(time->tm_wday + 1),
                      dec2bcd(time->tm_mday),          dec2bcd(time->tm_mon + 1), dec2bcd(time->tm_year - 100)};
    DS1307_LOCK(ds1307->mutex);
    err = set_register(ds1307, TIME_REG, buf, 7);
    DS1307_UNLOCK(ds1307->mutex);
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}

esp_err_t ds1307_enable_squarewave(ds1307_handle_t *ds1307, bool enable) {
    CHECK_ARG(ds1307);

    esp_err_t err;

    DS1307_LOCK(ds1307->mutex);
    err = update_register(ds1307, CONTROL_REG, SQWE_MASK, enable ? SQWE_BIT : 0);
    DS1307_UNLOCK(ds1307->mutex);
    if (err != ESP_OK) {
        return err;
    }
    return ESP_OK;
}

esp_err_t ds1307_is_squarewave_enabled(ds1307_handle_t *ds1307, bool *sqw_en) {
    CHECK_ARG(ds1307 && sqw_en);

    esp_err_t err;
    uint8_t val;

    DS1307_LOCK(ds1307->mutex);
    err = get_register(ds1307, CONTROL_REG, &val, 1);
    DS1307_UNLOCK(ds1307->mutex);
    if (err != ESP_OK) {
        return err;
    }

    *sqw_en = val & SQWE_BIT;

    return ESP_OK;
}

esp_err_t ds1307_set_squarewave_freq(ds1307_handle_t *ds1307, ds1307_squarewave_freq_t freq) {
    CHECK_ARG(ds1307);

    esp_err_t err;
    DS1307_LOCK(ds1307->mutex);
    err = update_register(ds1307, CONTROL_REG, SQWEF_MASK, freq);
    DS1307_UNLOCK(ds1307->mutex);
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}

esp_err_t ds1307_get_squarewave_freq(ds1307_handle_t *ds1307, ds1307_squarewave_freq_t *sqw_freq) {
    CHECK_ARG(ds1307 && sqw_freq);

    esp_err_t err;
    uint8_t val;

    DS1307_LOCK(ds1307->mutex);
    err = get_register(ds1307, CONTROL_REG, &val, 1);
    DS1307_UNLOCK(ds1307->mutex);
    if (err != ESP_OK) {
        return err;
    }

    *sqw_freq = val & ~SQWEF_MASK;

    return ESP_OK;
}

esp_err_t ds1307_set_output(ds1307_handle_t *ds1307, bool value) {
    CHECK_ARG(ds1307);

    esp_err_t err;

    DS1307_LOCK(ds1307->mutex);
    err = update_register(ds1307, CONTROL_REG, OUT_MASK, value ? OUT_BIT : 0);
    DS1307_UNLOCK(ds1307->mutex);
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}

esp_err_t ds1307_get_output(ds1307_handle_t *ds1307, bool *out) {
    CHECK_ARG(ds1307 && out);

    esp_err_t err;
    uint8_t val;

    DS1307_LOCK(ds1307->mutex);
    err = get_register(ds1307, CONTROL_REG, &val, 1);
    DS1307_UNLOCK(ds1307->mutex);
    if (err != ESP_OK) {
        return err;
    }

    *out = val & OUT_BIT;

    return ESP_OK;
}

esp_err_t ds1307_write_ram(ds1307_handle_t *ds1307, uint8_t offset, uint8_t *buf, uint8_t len) {
    CHECK_ARG(ds1307 && buf);

    if (offset + len > RAM_SIZE)
        return ESP_ERR_NO_MEM;

    esp_err_t err;

    DS1307_LOCK(ds1307->mutex);
    err = set_register(ds1307, RAM_REG + offset, buf, len);
    DS1307_UNLOCK(ds1307->mutex);
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}

esp_err_t ds1307_read_ram(ds1307_handle_t *ds1307, uint8_t offset, uint8_t *buf, uint8_t len) {
    CHECK_ARG(ds1307 && buf);

    if (offset + len > RAM_SIZE)
        return ESP_ERR_NO_MEM;

    esp_err_t err;

    DS1307_LOCK(ds1307->mutex);
    err = get_register(ds1307, RAM_REG + offset, buf, len);
    DS1307_UNLOCK(ds1307->mutex);
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}