set(srcs "src/ds1307.c")
set(include_dir "include")
set(priv_requires "esp_driver_i2c")

idf_component_register(SRCS ${srcs}
    INCLUDE_DIRS ${include_dir}
    PRIV_REQUIRES ${priv_requires})
