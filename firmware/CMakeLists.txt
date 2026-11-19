cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD pico2_w CACHE STRING "Bench board")
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(acoustic_bench C CXX ASM)
pico_sdk_init()
add_library(acoustic_capture STATIC pdm_capture.c decimator.c)
target_compile_options(acoustic_capture PRIVATE -O2)
pico_generate_pio_header(acoustic_capture ${CMAKE_CURRENT_LIST_DIR}/pdm_capture.pio)
target_include_directories(acoustic_capture PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(acoustic_capture PUBLIC pico_stdlib hardware_pio hardware_dma hardware_clocks)

function(add_acoustic_usb name)
    add_executable(${name} usb_stream.c stream_frame.c)
    target_link_libraries(${name} PRIVATE acoustic_capture pico_stdio_usb)
    target_compile_options(${name} PRIVATE -O2)
    target_compile_definitions(${name} PRIVATE
        PICO_STDIO_USB_ENABLE_IRQ_BACKGROUND_TASK=0
        PICO_STDIO_USB_ENABLE_RESET_VIA_BAUD_RATE=0
        PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE=0)
    pico_add_extra_outputs(${name})
endfunction()

add_acoustic_usb(acoustic_synthetic)
option(ACOUSTIC_ENABLE_MIC "Build a live microphone image with verified wiring" OFF)
if(ACOUSTIC_ENABLE_MIC)
    foreach(setting ACOUSTIC_CLOCK_GPIO ACOUSTIC_DATA_GPIO ACOUSTIC_SAMPLE_HIGH ACOUSTIC_SETTLE_WORDS)
        if(NOT DEFINED ${setting} OR NOT "${${setting}}" MATCHES "^(0|[1-9][0-9]*)$")
            message(FATAL_ERROR "${setting} must be an explicit nonnegative decimal integer")
        endif()
    endforeach()
    foreach(pin ACOUSTIC_CLOCK_GPIO ACOUSTIC_DATA_GPIO)
        if(NOT "${${pin}}" MATCHES "^([0-9]|1[0-9]|2[0-2]|2[6-8])$")
            message(FATAL_ERROR "${pin} must be a Pico header GPIO (0â€“22 or 26â€“28)")
        endif()
    endforeach()
    if(ACOUSTIC_SETTLE_WORDS LESS 1
        OR ACOUSTIC_CLOCK_GPIO EQUAL ACOUSTIC_DATA_GPIO
        OR ACOUSTIC_SAMPLE_HIGH GREATER 1 OR ACOUSTIC_SETTLE_WORDS GREATER 100000)
        message(FATAL_ERROR "Invalid microphone capture configuration")
    endif()
    add_acoustic_usb(acoustic_microphone)
    target_compile_definitions(acoustic_microphone PRIVATE ACOUSTIC_MIC=1
        ACOUSTIC_CLOCK_GPIO=${ACOUSTIC_CLOCK_GPIO}
        ACOUSTIC_DATA_GPIO=${ACOUSTIC_DATA_GPIO}
        ACOUSTIC_SAMPLE_HIGH=${ACOUSTIC_SAMPLE_HIGH}
        ACOUSTIC_SETTLE_WORDS=${ACOUSTIC_SETTLE_WORDS})
endif()
