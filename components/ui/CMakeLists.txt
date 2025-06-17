idf_component_register(SRCS screens/ui_Screen1.c
                            ui.c
                            components/ui_comp_hook.c
                            ui_helpers.c
                            images/ui_img_1706436910136_b31bcb12_png.c
                      INCLUDE_DIRS "."
                      REQUIRES lvgl__lvgl
                      )