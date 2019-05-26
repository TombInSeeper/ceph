cmake_minimum_required(VERSION 2.8.3)
project(ocssd_baidu C)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
set(NVM_VERSION_MAJOR 0 CACHE INTEGER "nvm_ver: NVM_VERSION_MAJOR")
set(NVM_VERSION_MINOR 1 CACHE INTEGER "nvm_ver: NVM_VERSION_MINOR")
set(NVM_VERSION_PATCH 0 CACHE INTEGER "nvm_ver: NVM_VERSION_PATH")
set(NVM_VERSION "${NVM_VERSION_MAJOR}.${NVM_VERSION_MINOR}.${NVM_VERSION_PATCH}")

include(FeatureSummary)
include(CheckLibraryExists)
include(CheckFunctionExists)

# Add versioning
add_definitions(-DLNVM_VERSION_MAJOR=${NVM_VERSION_MAJOR})
add_definitions(-DLNVM_VERSION_MINOR=${NVM_VERSION_MINOR})
add_definitions(-DLNVM_VERSION_PATCH=${NVM_VERSION_PATCH})

add_compile_options("-std=gnu99")
add_compile_options("-Wall")
add_compile_options("-Wextra")
add_compile_options("-march=native")
add_compile_options("-O2")

add_definitions("-D__CHECK_ENDIAN__")
add_definitions("-D_GNU_SOURCE")

if(GCC_AVX)
    message("Enable gcc option -mavx2")
    add_definitions("-mavx2")
endif()

set(NVM_OMP_ENABLED TRUE CACHE BOOL "nvm_omp: Compile with OpenMP?")
if(NVM_OMP_ENABLED)
	add_definitions(-DNVM_BE_SYSFS_ENABLED)

	if (NOT CMAKE_VERSION VERSION_LESS "2.8.12")
		find_package(OpenMP)
		if (OPENMP_FOUND)
		set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
		endif()
	endif()
endif()

check_library_exists(c clock_gettime "" LIBC_HAS_CLOCK_GETTIME)
check_library_exists(rt clock_gettime "time.h" LIBRT_HAS_CLOCK_GETTIME)

exec_program(
    "git"
    ${CMAKE_CURRENT_SOURCE_DIR}
    ARGS "log --format='%h' -1"
    OUTPUT_VARIABLE VERSION_SHA1
	RETURN_VALUE GIT_RETURN
)
if (0 EQUAL ${GIT_RETURN})
    message("git-sha: " ${VERSION_SHA1})
    add_definitions( -DGIT_SHA1="${VERSION_SHA1}" )
else()
    message("git-sha: null")
    add_definitions( -DGIT_SHA1="null" )
endif ()

include_directories(src/be_nvme)
include_directories(src/be_lightnvm)

set(HEADER_FILES
	include/liblightnvm.h
		include/nvm_debug.h
	src/nvm_spec.h
	src/nvm_vblk.h
		#---------------------------
	objssd-nvme.h
    )

set(SOURCE_FILES
	src/nvm_spec.c
	src/nvm_be.c
	src/be_lightnvm/nvm_be_ioctl.c
	src/nvm_dev.c
	src/nvm_buf.c
	src/nvm_bbt.c
   	src/nvm_cmd.c
	src/nvm_addr.c
	src/nvm_vblk.c
    	src/be_nvme/nvm_baidu.c
	src/be_nvme/nvm_async.c
    	src/be_nvme/nvm_cmb.c
	src/be_nvme/nvm_aer.c



#----------------------------
	objssd-nvme.c
	)

include_directories(utils)
include_directories(include)
include_directories(src/)

set(LNAME "lightnvm")

add_library(${LNAME} STATIC ${HEADER_FILES} ${SOURCE_FILES})
target_link_libraries(${LNAME} aio pthread rt tcmalloc)
set_target_properties(${LNAME} PROPERTIES OUTPUT_NAME ${LNAME})

add_library(lightnvm_so SHARED ${HEADER_FILES} ${SOURCE_FILES})
target_link_libraries(lightnvm_so aio pthread rt tcmalloc)
set_target_properties(lightnvm_so PROPERTIES OUTPUT_NAME ${LNAME})


install(TARGETS ${LNAME} DESTINATION lib COMPONENT lib)
install(FILES "${PROJECT_SOURCE_DIR}/include/liblightnvm.h"
    DESTINATION include COMPONENT dev)
