set(srcs "src/dht11.c" "src/dht_onewire_rmt_interface.c")
set(include_dirs "include" "interface")
set(priv_requires "esp_driver_rmt" "esp_driver_gpio")

idf_component_register(SRCS ${srcs}
    INCLUDE_DIRS ${include_dirs}
    PRIV_REQUIRES ${priv_requires})
