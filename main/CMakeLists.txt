idf_component_register(SRCS "scoreboard.c" "hub75_display.c" "main.c" "esp_hid_gap.c"
                    PRIV_REQUIRES bt nvs_flash esp_hid driver esp_timer
                    INCLUDE_DIRS ".")
