idf_component_register(SRCS "main.c" "screen.c" "wasm.c" "utils.c"
                    INCLUDE_DIRS "."
                    REQUIRES lvgl lvgl_esp32_drivers wasm3 spi_flash)
