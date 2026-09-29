##
# bsp — 立创实战派 ESP32-S3 板级支持包
#
# 依赖的托管组件通过 idf_component.yml 自动解析；
# 这里只声明 ESP-IDF 内置组件。
##

set(srcs
    "src/bsp_board.c"
    "src/bsp_i2c.c"
    "src/bsp_io_expander.c"
    "src/bsp_display.c"
)

if(CONFIG_BSP_ENABLE_TOUCH)
    list(APPEND srcs "src/bsp_touch.c")
endif()

if(CONFIG_BSP_ENABLE_LVGL)
    list(APPEND srcs "src/bsp_lvgl.c")
endif()

if(CONFIG_BSP_ENABLE_CAMERA)
    list(APPEND srcs "src/bsp_camera.c")
endif()

if(CONFIG_BSP_ENABLE_AUDIO)
    list(APPEND srcs "src/bsp_audio.c")
endif()

if(CONFIG_BSP_ENABLE_IMU)
    list(APPEND srcs "src/bsp_imu.c")
endif()

if(CONFIG_BSP_ENABLE_STORAGE)
    list(APPEND srcs "src/bsp_storage.c")
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "include"
    REQUIRES esp_lcd driver fatfs spiffs sdmmc
    PRIV_REQUIRES esp_timer
)
