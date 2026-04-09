cmake_minimum_required(VERSION 3.28)
project(yaponmdl VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 23)

if (MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /EHsc /Zi")
    set(CMAKE_CXX_FLAGS_DEBUG "/EHsc /Zi /D_CRTDBG_MAP_ALLOC")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -g")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
endif()

include(FetchContent)
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/third_party)

FetchContent_Declare(
    glm
    REQUIRED
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG a532f5b1cf27d6a3c099437e6959cf7e398a0a67 # 1.0.2
)

FetchContent_Declare(
    fmt
    REQUIRED
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 40626af88bd7df9a5fb80be7b25ac85b122d6c21 # 11.2.0
)

FetchContent_MakeAvailable(glm fmt)

add_compile_definitions(
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    GLFW_INCLUDE_NONE
)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(PROGRAM_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/src)

add_library(thirdparty OBJECT ${PROGRAM_SRC_DIR}/thirdparty.cpp)

set(SOURCES_LIST
    ${PROGRAM_SRC_DIR}/main.cpp
    ${PROGRAM_SRC_DIR}/common/binaryreader.cpp
    ${PROGRAM_SRC_DIR}/common/byteutils.cpp
    ${PROGRAM_SRC_DIR}/formats/gmo.cpp
    ${PROGRAM_SRC_DIR}/formats/gxt.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES_LIST})

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
    target_compile_options(thirdparty PRIVATE /W0)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE 
        -Wall 
        -Wextra 
        -Wpedantic
    )
    target_compile_options(thirdparty PRIVATE -w)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:thirdparty> fmt glm)