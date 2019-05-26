cmake_minimum_required(VERSION 2.8)

include_directories("${CMAKE_SOURCE_DIR}/src")
include_directories("${CMAKE_SOURCE_DIR}/include")
include_directories("${CMAKE_SOURCE_DIR}/include/linux/uapi")
include_directories("${CMAKE_SOURCE_DIR}/utils")
include_directories("${CMAKE_SOURCE_DIR}/utils/cmocka")
execute_process(COMMAND cp ${CMAKE_SOURCE_DIR}/tests/pci_power_off.py ./tests)
execute_process(COMMAND cp ${CMAKE_SOURCE_DIR}/tests/pci_power_on.py ./tests)

set(SOURCE_FILES
    ../utils/hal_timer.c
    ../utils/log_csv.c
    ../utils/cmocka/cmocka.c
    test_dev.c
    test_addr_conv.c
    test_vblk.c
    test_bbt.c
    test_ext_cmd.c
    test_ppa.c
    test_async.c
    test_cmb.c
    test_aer.c
    test_main.c
    test_fio_basic.c
    test_fio_stress.c
    test_fio_mixed.c
    test_fio_workload.c
    test_power_loss.c
    test_io_status.c
    test_reset.c
    )

add_executable(nvm_test ${SOURCE_FILES})
target_link_libraries(nvm_test pthread m ${LNAME})
install(TARGETS nvm_test DESTINATION bin COMPONENT tests)
