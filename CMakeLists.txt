cmake_minimum_required(VERSION 3.16)
project(onekvm_extension_easytier LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_path(JSON_C_INCLUDE_DIR json-c/json.h)
find_library(JSON_C_LIBRARY json-c)
if(NOT JSON_C_INCLUDE_DIR OR NOT JSON_C_LIBRARY)
  message(FATAL_ERROR "json-c development files are required")
endif()

add_executable(onekvm-easytier-controller src/main.cpp)
target_compile_options(onekvm-easytier-controller PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(onekvm-easytier-controller PRIVATE ${JSON_C_INCLUDE_DIR})
target_link_libraries(onekvm-easytier-controller PRIVATE ${JSON_C_LIBRARY} m)

install(TARGETS onekvm-easytier-controller RUNTIME DESTINATION bin)

include(CTest)
if(BUILD_TESTING)
  add_test(NAME controller-lifecycle
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/controller-test.sh
            $<TARGET_FILE:onekvm-easytier-controller>)
endif()
