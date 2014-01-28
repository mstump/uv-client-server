CMAKE_MINIMUM_REQUIRED(VERSION 2.8.0)
set(PROJECT_NAME_STR uv)
project(${PROJECT_NAME_STR} C CXX)

#-------------------------------------------------------------------------------
# The version number
#-------------------------------------------------------------------------------
set (uv_VERSION_MAJOR 0)
set (uv_VERSION_MINOR 1)
set (uv_VERSION_STRING ${uv_VERSION_MAJOR}.${uv_VERSION_MINOR})

# libuv
set(LIBUV_INCLUDE_NAMES uv.h)
set(LIBUV_LIBRARY_NAMES libuv.dylib)

find_path(LIBUV_INCLUDE_DIRS NAMES ${LIBUV_INCLUDE_NAMES} HINTS /usr/include /usr/local/include)
find_library(LIBUV_LIBRARIES NAMES ${LIBUV_LIBRARY_NAMES} HINTS /usr/include /usr/local/include)

if(LIBUV_INCLUDE_DIRS AND LIBUV_LIBRARIES)
  set(LIBUV_FOUND TRUE)
  set(INCLUDES ${INCLUDES} ${LIBUV_INCLUDE_DIR} )
  set(LIBS ${LIBS} ${LIBUV_LIBRARIES} )
  message(STATUS "Found LIBUV: ${LIBUV_LIBRARIES}")
else(LIBUV_INCLUDE_DIRS AND LIBUV_LIBRARIES)
  message(FATAL_ERROR "Could not find LIBUV library.")
endif(LIBUV_INCLUDE_DIRS AND LIBUV_LIBRARIES)

find_package(ZLIB REQUIRED)
set(LIBS ${LIBS} ${ZLIB_LIBRARIES})
set(INCLUDES ${INCLUDES} ${ZLIB_INCLUDE_DIR} )

find_package(OpenSSL REQUIRED)
set(LIBS ${LIBS} ${OPENSSL_LIBRARIES})
set(INCLUDES ${INCLUDES} ${OPENSSL_INCLUDE_DIR} )


include_directories(${INCLUDES} "include/")
set(PROJECT_COMPILER_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -fPIC -Wall -pedantic -Wextra -Werror -Wno-long-long -Wno-c++11-narrowing -Wno-deprecated-declarations")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "build/bin/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build/bin)

file(GLOB SRC_FILES ${PROJECT_SOURCE_DIR}/src/server/*.cpp)
add_executable(${PROJECT_NAME}-server ${SRC_FILES})
target_link_libraries(${PROJECT_NAME}-server ${LIBS})

file(GLOB SRC_FILES ${PROJECT_SOURCE_DIR}/src/client/*.cpp)
add_executable(${PROJECT_NAME}-client ${SRC_FILES})
target_link_libraries(${PROJECT_NAME}-client ${LIBS})

file(GLOB SRC_FILES ${PROJECT_SOURCE_DIR}/src/test/*.cpp)
add_executable(${PROJECT_NAME}-test ${SRC_FILES})
target_link_libraries(${PROJECT_NAME}-test ${LIBS})

set_property(
  TARGET ${PROJECT_NAME}-server
  APPEND PROPERTY COMPILE_FLAGS ${PROJECT_COMPILER_FLAGS})

set_property(
  TARGET ${PROJECT_NAME}-client
  APPEND PROPERTY COMPILE_FLAGS ${PROJECT_COMPILER_FLAGS})

set_property(
  TARGET ${PROJECT_NAME}-test
  APPEND PROPERTY COMPILE_FLAGS ${PROJECT_COMPILER_FLAGS})
