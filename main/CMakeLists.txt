idf_component_register(SRC_DIRS "./"
                       INCLUDE_DIRS "./"
                       REQUIRES ui common)

set_source_files_properties(
    ${LV_DEMOS_SOURCES}
    PROPERTIES COMPILE_OPTIONS
    -DLV_LVGL_H_INCLUDE_SIMPLE)

if (NOT CONFIG_KEY_PRESS_DIALOG_MODE)
spiffs_create_partition_image(spiffs_data ../spiffs FLASH_IN_PROJECT)
endif()

