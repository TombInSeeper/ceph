cmake_minimum_required(VERSION 2.8)
set(CLI true CACHE BOOL "CLI: Build command-line interface (CLI) tools")
if (NOT CLI)
    return()
endif ()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-function")
include_directories("${CMAKE_SOURCE_DIR}/include")
include_directories("${CMAKE_SOURCE_DIR}/src")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fopenmp")

set(COMMON_FILES
    nvm_cli.c
    ../utils/hal_timer.c
    )

set(SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/cli_dev.c
    ${CMAKE_CURRENT_SOURCE_DIR}/cli_cmd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/cli_bbt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/cli_addr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/cli_scan_disk.c
 #   ${CMAKE_CURRENT_SOURCE_DIR}/cli_vblk.c
    )

link_directories(${PROJECT_BINARY_DIR})
#
# static linking, against lightnvm_a, to avoid runtime dependency on liblightnvm
#
foreach (SRC_FN ${SOURCE_FILES})
    get_filename_component(SRC_FN_WE ${SRC_FN} NAME_WE)
    string(REPLACE "cli_" "" SRC_FN_WE "${SRC_FN_WE}")
    set(EXE_FN "nvm_${SRC_FN_WE}")
    add_executable(${EXE_FN} ${SRC_FN} ${COMMON_FILES})
    target_link_libraries(${EXE_FN} lightnvm)
    target_include_directories(${EXE_FN} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    install(TARGETS ${EXE_FN} DESTINATION bin COMPONENT cli)
endforeach ()

