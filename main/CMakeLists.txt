idf_component_register(SRC_DIRS "./"
                       INCLUDE_DIRS "./"
                       REQUIRES ui common esp_wifi esp_http_server nvs_flash json spiffs)

set_source_files_properties(
    ${LV_DEMOS_SOURCES}
    PROPERTIES COMPILE_OPTIONS
    -DLV_LVGL_H_INCLUDE_SIMPLE)

# 移除条件限制，确保SPIFFS镜像始终被创建
spiffs_create_partition_image(spiffs_data ../spiffs FLASH_IN_PROJECT)
