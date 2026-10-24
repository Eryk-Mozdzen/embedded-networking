cmake_minimum_required(VERSION 3.16)

set(CMAKE_TOOLCHAIN_FILE "toolchain.cmake")

project(generator)

set(TARGET_ELF ${CMAKE_PROJECT_NAME}.elf)
set(TARGET_HEX ${CMAKE_PROJECT_NAME}.hex)
set(TARGET_DUMP ${CMAKE_PROJECT_NAME}.dump)

add_executable(${TARGET_ELF}
	"src/main.c"

    "uip/uip/psock.c"
    "uip/uip/timer.c"
    "uip/uip/uip_arp.c"
    "uip/uip/uip-fw.c"
    "uip/uip/uip-neighbor.c"
    "uip/uip/uip-split.c"
    "uip/uip/uip.c"
    "uip/uip/uiplib.c"

    "uip-slip/slipdev.c"
)

target_include_directories(${TARGET_ELF} PRIVATE
	"include"
    "uip/uip"
    "uip-slip"
)

target_compile_definitions(${TARGET_ELF} PRIVATE
    F_CPU=16000000UL
)

target_compile_options(${TARGET_ELF} PRIVATE
	-Os

    -std=c23
	-Wall
	-Wextra
	-Wpedantic

    -ffunction-sections
    -fdata-sections
	-mmcu=atmega32a
)

target_link_options(${TARGET_ELF} PRIVATE
    -Wl,--gc-sections
	-mmcu=atmega32a
)

add_custom_command(TARGET ${TARGET_ELF} POST_BUILD
	COMMAND ${CMAKE_OBJCOPY} -O ihex ${TARGET_ELF} ${TARGET_HEX}
	COMMAND ${CMAKE_OBJDUMP} -j .text -D ${TARGET_ELF} > ${TARGET_DUMP}
	COMMAND ${CMAKE_SIZE} ${TARGET_ELF}
)

add_custom_target(flash
	avrdude -p atmega32a -c usbasp -e -U flash:w:${TARGET_HEX}:i
	DEPENDS ${TARGET_ELF}
)
