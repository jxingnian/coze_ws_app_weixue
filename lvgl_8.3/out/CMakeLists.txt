SET(SOURCES components/ui_comp_textarea2.c
    components/ui_comp.c
    screens/ui_Main.c
    screens/ui_Main1.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    fonts/ui_font_battery.c
    fonts/ui_font_dinglie16.c
    fonts/ui_font_dinglie32.c
    fonts/ui_font_headPortrait.c
    fonts/ui_font_network.c
    fonts/ui_font_status.c)

add_library(ui ${SOURCES})
