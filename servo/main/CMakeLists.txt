idf_component_register(SRCS "main.c"
        PRIV_REQUIRES spi_flash
        REQUIRES driver bt nvs_flash
        INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=unused-const-variable)
