cmake_minimum_required (VERSION 2.8)

set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}")
# this is required
SET(CMAKE_SYSTEM_NAME Linux)

#[[
# specify the cross compiler
SET(CMAKE_C_COMPILER   arm-linux-gnueabihf-gcc)
SET(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

find_program(toolchain_path arm-linux-gnueabihf-gcc PATH)

string(REGEX REPLACE "(.+)\\bin/arm-linux.*" "\\1" toolchain_root_dir ${toolchain_path})

set(CMAKE_CROSSCOMPILING TRUE)
# where is the target environment 
SET(CMAKE_FIND_ROOT_PATH  ${toolchain_root_dir}/arm-linux-gnueabihf/)

# search for programs in the build host directories (not necessary)
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
]]
project(cbinder)

set(CMAKE_CXX_FLAGS "-std=gnu++11 -fPIC -DPIC")
set(CMAKE_C_FLAGS "-fPIC -DPIC -Wall -O3 -g")


include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include 
                    )

file(GLOB_RECURSE CBINDER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)

add_library(cbinder SHARED ${CBINDER_SRC})
target_link_libraries(cbinder pthread)

add_executable(service_manager service_manager/binder_srv_manager.c)
target_link_libraries(service_manager cbinder pthread)

add_executable(led_client test/led_client.c)
target_link_libraries(led_client cbinder pthread)

add_executable(led_service test/led_service.c)
target_link_libraries(led_service cbinder pthread)

add_executable(fd_client test/fd_client.c)
target_link_libraries(fd_client cbinder pthread)

add_executable(fd_service test/fd_service.c)
target_link_libraries(fd_service cbinder pthread)