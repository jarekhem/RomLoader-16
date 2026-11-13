include_guard(GLOBAL)

set(ROMLOADER_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE INTERNAL "")
get_filename_component(ROMLOADER_ROOT_DIR "${ROMLOADER_CMAKE_DIR}/.." ABSOLUTE CACHE INTERNAL "")
set(ROMLOADER_SDK_DIR "${ROMLOADER_ROOT_DIR}/sdk" CACHE INTERNAL "")
set(ROMLOADER_SEND_SCRIPT "${ROMLOADER_ROOT_DIR}/tools/send_payload.py" CACHE INTERNAL "")

find_package(Python3 COMPONENTS Interpreter QUIET)
if (Python3_Interpreter_FOUND)
    set(ROMLOADER_PYTHON "${Python3_EXECUTABLE}" CACHE INTERNAL "")
else ()
    set(ROMLOADER_PYTHON "python" CACHE INTERNAL "")
endif ()

# ==============================================================================
# add_romloader_payload(
#     TARGET    <target_name>
#     [SOURCES   <src1> <src2> ...]
#     [LIBRARIES <lib1> <lib2> ...]
#     [INCLUDES  <dir1> <dir2> ...]
#     [PORT      COM6]
#     [BAUD      115200]
#     [SEG       1000]
#     [OFF       0000]
#     [TCP       localhost:4444]
# )
#
# Creates a RAM-loadable 8086 binary target (<TARGET>.bin) alongside the main
# project targets, plus helper targets for uploading and running via RomLoader-16:
#   - upload_<TARGET>    : Upload <TARGET>.bin to hardware RAM (without jumping)
#   - run_hw_<TARGET>    : Upload & immediately execute <TARGET>.bin on hardware
#   - run_bochs_<TARGET> : Upload & execute <TARGET>.bin in Bochs via TCP
# ==============================================================================
function(add_romloader_payload)
    set(options "")
    set(oneValueArgs TARGET PORT BAUD SEG OFF TCP LINKER_SCRIPT ENTRY_ASM)
    set(multiValueArgs SOURCES LIBRARIES INCLUDES COMPILE_OPTIONS)
    cmake_parse_arguments(RL "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT RL_TARGET)
        message(FATAL_ERROR "add_romloader_payload: TARGET argument is required")
    endif ()

    if (NOT RL_PORT)
        set(RL_PORT "COM6")
    endif ()
    if (NOT RL_BAUD)
        set(RL_BAUD "115200")
    endif ()
    if (NOT RL_SEG)
        set(RL_SEG "1000")
    endif ()
    if (NOT RL_OFF)
        set(RL_OFF "0000")
    endif ()
    if (NOT RL_TCP)
        set(RL_TCP "localhost:4444")
    endif ()
    if (NOT RL_LINKER_SCRIPT)
        set(RL_LINKER_SCRIPT "${ROMLOADER_SDK_DIR}/ram_payload.ld")
    endif ()
    if (NOT RL_ENTRY_ASM)
        set(RL_ENTRY_ASM "${ROMLOADER_SDK_DIR}/boot_ram.asm")
    endif ()

    enable_language(ASM_NASM)

    add_executable(${RL_TARGET}
        "${RL_ENTRY_ASM}"
        ${RL_SOURCES}
    )

    target_include_directories(${RL_TARGET} PUBLIC
        "${ROMLOADER_SDK_DIR}"
        ${RL_INCLUDES}
    )

    if (RL_LIBRARIES)
        target_link_libraries(${RL_TARGET} PRIVATE ${RL_LIBRARIES})
    endif ()

    target_compile_options(${RL_TARGET} PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-ffreestanding>
        $<$<COMPILE_LANGUAGE:C>:-Os>
        $<$<COMPILE_LANGUAGE:C>:-ffunction-sections>
        $<$<COMPILE_LANGUAGE:C>:-fdata-sections>
        ${RL_COMPILE_OPTIONS}
    )

    target_link_options(${RL_TARGET} PRIVATE
        "-nostdlib"
        "-T${RL_LINKER_SCRIPT}"
        "-Wl,-Map=${RL_TARGET}.map"
        "-Wl,--gc-sections"
    )

    set(BIN_FILE "${CMAKE_CURRENT_BINARY_DIR}/${RL_TARGET}.bin")

    add_custom_command(TARGET ${RL_TARGET} POST_BUILD
        COMMAND ia16-elf-objcopy -O binary $<TARGET_FILE:${RL_TARGET}> "${BIN_FILE}"
        COMMENT "Generating RomLoader-16 RAM payload image (${RL_TARGET}.bin)..."
    )

    add_custom_target(upload_${RL_TARGET}
        COMMAND "${ROMLOADER_PYTHON}" "${ROMLOADER_SEND_SCRIPT}"
                --port "${RL_PORT}" --baud "${RL_BAUD}"
                --seg "${RL_SEG}" --off "${RL_OFF}"
                "${BIN_FILE}"
        DEPENDS ${RL_TARGET}
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        USES_TERMINAL
        COMMENT "Uploading ${RL_TARGET}.bin to hardware (${RL_PORT} -> ${RL_SEG}:${RL_OFF})..."
    )

    add_custom_target(run_hw_${RL_TARGET}
        COMMAND "${ROMLOADER_PYTHON}" "${ROMLOADER_SEND_SCRIPT}"
                --port "${RL_PORT}" --baud "${RL_BAUD}"
                --seg "${RL_SEG}" --off "${RL_OFF}"
                --run "${BIN_FILE}"
        DEPENDS ${RL_TARGET}
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        USES_TERMINAL
        COMMENT "Uploading & executing ${RL_TARGET}.bin on hardware (${RL_PORT} -> ${RL_SEG}:${RL_OFF})..."
    )

    if (NOT TARGET run_hw)
        add_custom_target(run_hw DEPENDS run_hw_${RL_TARGET})
    endif ()

    add_custom_target(run_bochs_${RL_TARGET}
        COMMAND "${ROMLOADER_PYTHON}" "${ROMLOADER_SEND_SCRIPT}"
                --tcp "${RL_TCP}"
                --seg "${RL_SEG}" --off "${RL_OFF}"
                --run "${BIN_FILE}"
        DEPENDS ${RL_TARGET}
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        USES_TERMINAL
        COMMENT "Uploading & executing ${RL_TARGET}.bin in Bochs (${RL_TCP} -> ${RL_SEG}:${RL_OFF})..."
    )
endfunction()
