idf_component_register(SRCS "main.c" "lcd_jr.c" "mqtt/mqtt.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_idf_lib_helpers esp_wifi nvs_flash esp_event 
                                                                  esp_netif mqtt
                    PRIV_REQUIRES freertos lora)
