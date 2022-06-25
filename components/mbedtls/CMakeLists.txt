# Dummy. Mongoose OS provides its own mbedTLS.

idf_build_get_property(target IDF_TARGET)

idf_component_register(SRCS "port/md/esp_md.c")
