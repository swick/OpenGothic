cmake_minimum_required(VERSION 2.8)

project(Gothic2Notr)
set (CMAKE_CXX_STANDARD 14)

include_directories("Game")

set(CMAKE_DEBUG_POSTFIX "")

if(POLICY CMP0990)
  cmake_policy(SET CMP0990 OLD)
endif()

if(POLICY CMP0015)
  cmake_policy(SET CMP0015 OLD)
endif()

set(CMAKE_BINARY_DIR ../)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")

file(GLOB SOURCES
    "Game/*.h"
    "Game/*.cpp"
    "Game/**/*.h"
    "Game/**/*.cpp"
    "Game/**/**/*.h"
    "Game/**/**/*.cpp"
    "Game/**/**/**/*.h"
    "Game/**/**/**/*.cpp"
  )

if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
  link_directories(${PROJECT_NAME} "$ENV{VK_SDK_PATH}/lib")
else()
  link_directories(${PROJECT_NAME} "$ENV{VK_SDK_PATH}/Lib32")
endif()

link_directories(${PROJECT_NAME} "$ENV{VK_SDK_PATH}/lib")

add_executable(${PROJECT_NAME} ${SOURCES} icon.rc )

add_subdirectory(lib/edd-dbg)
include_directories(lib/edd-dbg/include)
if(WIN32)
  target_link_libraries(${PROJECT_NAME} edd_dbg)
endif()

add_subdirectory(lib/ZenLib)
include_directories(lib/ZenLib)
target_link_libraries(${PROJECT_NAME} zenload daedalus)

add_subdirectory(lib/MoltenTempest/Engine)
include_directories(lib/MoltenTempest/Engine/include)
target_link_libraries(${PROJECT_NAME} MoltenTempest)

set(BULLET2_MULTITHREADING ON)
set(USE_GRAPHICAL_BENCHMARK OFF CACHE STRING "" FORCE)
set(BUILD_BULLET2_DEMOS OFF CACHE STRING "" FORCE) # No samples
set(BUILD_OPENGL3_DEMOS OFF CACHE STRING "" FORCE) # No samples
set(BUILD_UNIT_TESTS OFF CACHE STRING "" FORCE) # No tests
set(BUILD_CPU_DEMOS OFF CACHE STRING "" FORCE) # No samples
set(BUILD_EXTRAS OFF CACHE STRING "" FORCE) # No bugs
set(GLFW_BUILD_EXAMPLES OFF CACHE STRING "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE STRING "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE STRING "" FORCE)
set(BUILD_BULLET3 OFF CACHE STRING "" FORCE) # Can use bullet2, bullet3 wants to build examples...
add_subdirectory(lib/bullet3)
include_directories(lib/bullet3/src)

if(WIN32)
  target_link_libraries(${PROJECT_NAME} shlwapi DbgHelp)
elseif(UNIX)
  target_link_libraries(${PROJECT_NAME} -lpthread)
endif()

target_link_libraries(${PROJECT_NAME} BulletDynamics BulletCollision LinearMath)

