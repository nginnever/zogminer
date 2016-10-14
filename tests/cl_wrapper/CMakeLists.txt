cmake_minimum_required(VERSION 3.5.1)

project(cl_zogminer)

find_package(OpenCL REQUIRED)
#include_directories(${OpenCL_INCLUDE_DIRS})
#link_directories(${OpenCL_INCLUDE_DIRS})
#link_directories(${OpenCL_LIBRARY})

# A custom command and target to turn the OpenCL kernel into a byte array header
# The normal build depends on it properly and if the kernel file is changed, then
# a rebuild of this directory should be triggered
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/cl_zogminer_kernel.h
	COMMAND ${CMAKE_COMMAND} ARGS
	-DBIN2H_SOURCE_FILE="${CMAKE_CURRENT_SOURCE_DIR}/cl_zogminer_kernel.cl"
	-DBIN2H_VARIABLE_NAME=cl_miner_kernel
	-DBIN2H_HEADER_FILE="${CMAKE_CURRENT_SOURCE_DIR}/cl_zogminer_kernel.h"
	-P "${CMAKE_CURRENT_SOURCE_DIR}/bin2h.cmake"
	COMMENT "Generating OpenCL Kernel Byte Array"
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/cl_zogminer_kernel.cl
)
add_custom_target(clbin2h DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/cl_zogminer_kernel.h ${CMAKE_CURRENT_SOURCE_DIR}/cl_zogminer_kernel.cl)

file(GLOB SOURCES "*.cpp")


add_executable (test_cl_zogminer ${SOURCES})
target_link_libraries(test_cl_zogminer ${OpenCL_LIBRARIES})
add_dependencies(test_cl_zogminer clbin2h)
