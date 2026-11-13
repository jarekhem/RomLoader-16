cmake_minimum_required(VERSION 3.15)

set(PROJECT_NAME RomLoader-16)
project(${PROJECT_NAME} C ASM_NASM)

set(CMAKE_ASM_NASM_FLAGS "${CMAKE_ASM_NASM_FLAGS} -f elf")

function(define_filename_macro target)
    get_target_property(sources ${target} SOURCES)
    if (sources)
        foreach(src IN LISTS sources)
            get_filename_component(basename ${src} NAME)
            set_property(SOURCE ${src} APPEND PROPERTY COMPILE_DEFINITIONS "__FILENAME__=\"${basename}\"")
        endforeach()
    endif()
endfunction()

add_executable(${PROJECT_NAME}
        src/main.c
        src/vga.c
        src/kbd.c
        src/uart.c
        src/loader.c
        src/stdfunc.c
        src/boot.asm
        src/systemInit.asm
        src/vectors.asm
        src/int10.asm
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

define_filename_macro(${PROJECT_NAME})

target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-ffreestanding>
        $<$<COMPILE_LANGUAGE:C>:-Os>
        $<$<COMPILE_LANGUAGE:C>:-ffunction-sections>
        $<$<COMPILE_LANGUAGE:C>:-fdata-sections>
        $<$<COMPILE_LANGUAGE:ASM_NASM>:-I${CMAKE_CURRENT_SOURCE_DIR}/src/>
)

target_link_options(${PROJECT_NAME} PRIVATE
        "-nostdlib"
        "-T${CMAKE_CURRENT_SOURCE_DIR}/bios.ld"
        "-Wl,-Map=mapfile.map"
        "-Wl,--gc-sections"
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ia16-elf-objcopy -O binary ${PROJECT_NAME} ${PROJECT_NAME}.bin
        COMMENT "Generating 32KB ROM image (${PROJECT_NAME}.bin)..."
)

add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/hello_payload.bin"
        COMMAND ${CMAKE_ASM_NASM_COMPILER} -f bin "${CMAKE_CURRENT_SOURCE_DIR}/payloads/hello_payload.asm" -o "${CMAKE_CURRENT_BINARY_DIR}/hello_payload.bin"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/payloads/hello_payload.asm"
        COMMENT "Assembling sample RAM payload (hello_payload.bin)..."
)

add_custom_target(hello_payload ALL
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/hello_payload.bin"
)

add_custom_target(
        run_bochs
        COMMAND "bochs" -q -f "${CMAKE_CURRENT_SOURCE_DIR}/bochsrc.bxrc"
        DEPENDS ${PROJECT_NAME}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Launching Bochs Emulator (COM1 listening on localhost:4444)..."
)

add_custom_target(
        debug_bochs
        COMMAND "bochs" -q -debugger -f "${CMAKE_CURRENT_SOURCE_DIR}/bochsrc.bxrc"
        DEPENDS ${PROJECT_NAME}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Launching Bochs Emulator with GUI Debugger (COM1 on localhost:4444)..."
)
