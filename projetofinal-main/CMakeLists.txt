# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 1.5.1)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.0.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

# Diretório do FreeRTOSConfig.h (obrigatório para a porta RP2040)
set(FREERTOS_CONFIG_FILE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/src CACHE STRING "")

# Pull in FreeRTOS Kernel (porta RP2040 SMP - cria targets FreeRTOS-Kernel-Heap*)
include("C:/Users/antle/Downloads/FreeRTOSv202406.01-LTS/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake")

project(projeto_final C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# ==================== EXECUTÁVEL PRINCIPAL ====================
add_executable(projeto_final 
        projeto_final.c
        src/mpu6050/mpu6050.c
        src/aht10/aht10.c
        src/ssd1306/ssd1306.c
        src/servo/servo.c
        src/aes/aes.c
        src/wifi_module/wifi_module.c
        src/security_module/security_module.c
        src/atuadores_module/atuadores_module.c
        src/sensores_uart_module/sensores_uart_module.c
        src/mqtt_module/mqtt_module.c
)

pico_set_program_name(projeto_final "projeto_final")
pico_set_program_version(projeto_final "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(projeto_final 0)
pico_enable_stdio_usb(projeto_final 1)

# Add the standard library to the build

target_link_libraries(projeto_final PRIVATE
    FreeRTOS-Kernel-Heap4
    pico_stdlib
    hardware_i2c
    hardware_pwm
    hardware_gpio
    hardware_uart
    hardware_clocks
    pico_cyw43_arch_lwip_threadsafe_background
    pico_lwip_mqtt
)

# Add the standard include files to the build
target_include_directories(projeto_final PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/src
        ${CMAKE_CURRENT_LIST_DIR}/src/mpu6050
        ${CMAKE_CURRENT_LIST_DIR}/src/aht10
        ${CMAKE_CURRENT_LIST_DIR}/src/ssd1306
        ${CMAKE_CURRENT_LIST_DIR}/src/servo
        ${CMAKE_CURRENT_LIST_DIR}/src/aes
        ${CMAKE_CURRENT_LIST_DIR}/src/wifi_module
        ${CMAKE_CURRENT_LIST_DIR}/src/security_module
        ${CMAKE_CURRENT_LIST_DIR}/src/atuadores_module
        ${CMAKE_CURRENT_LIST_DIR}/src/sensores_uart_module
        ${CMAKE_CURRENT_LIST_DIR}/src/mqtt_module
)

pico_add_extra_outputs(projeto_final)
