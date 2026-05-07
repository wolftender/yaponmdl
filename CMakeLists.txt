cmake_minimum_required(VERSION 3.28)
project(yaponmdl VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

find_package(Python3 COMPONENTS Interpreter REQUIRED)

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
include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/third_party/glad/include)

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

# wxwidgets
find_package(wxWidgets CONFIG REQUIRED COMPONENTS core base stc gl)

add_compile_definitions(
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    GLM_ENABLE_EXPERIMENTAL
)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(GENERATED_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/include)
set(ASSET_OUTPUT_DIR ${GENERATED_INCLUDE_DIR}/generated)
include_directories(${GENERATED_INCLUDE_DIR})

set(PROGRAM_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/src)

add_library(thirdparty OBJECT ${PROGRAM_SRC_DIR}/thirdparty.cpp)
add_library(glad OBJECT ${CMAKE_SOURCE_DIR}/third_party/glad/src/glad.c)

set(GENERATED_HEADERS "")
function(add_asset ASSET_FILE HEADER_NAME)
    set(OUTPUT_FILE ${ASSET_OUTPUT_DIR}/${HEADER_NAME})
    add_custom_command(
        OUTPUT ${OUTPUT_FILE}
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/embed_file.py ${ASSET_FILE} ${OUTPUT_FILE}
        DEPENDS ${ASSET_FILE}
        COMMENT "embedding binary asset ${ASSET_FILE} -> ${OUTPUT_FILE}"
        VERBATIM
    )
    set(GENERATED_HEADERS ${GENERATED_HEADERS} ${OUTPUT_FILE} PARENT_SCOPE)
endfunction()

add_asset(${PROGRAM_SRC_DIR}/shaders/vert_texture.glsl vert_texture.glsl.h)
add_asset(${PROGRAM_SRC_DIR}/shaders/frag_texture.glsl frag_texture.glsl.h)
add_asset(${PROGRAM_SRC_DIR}/shaders/frag_checker.glsl frag_checker.glsl.h)
add_asset(${PROGRAM_SRC_DIR}/shaders/frag_mesh.glsl frag_mesh.glsl.h)
add_asset(${PROGRAM_SRC_DIR}/shaders/vert_staticmesh.glsl vert_staticmesh.glsl.h)
add_asset(${PROGRAM_SRC_DIR}/shaders/vert_skinmesh.glsl vert_skinmesh.glsl.h)
add_asset(${PROGRAM_SRC_DIR}/shaders/vert_postfilter.glsl vert_postfilter.glsl.h)
add_asset(${PROGRAM_SRC_DIR}/shaders/frag_postfilter.glsl frag_postfilter.glsl.h)
add_asset(${PROGRAM_SRC_DIR}/shaders/vert_text.glsl vert_text.glsl.h)
add_asset(${PROGRAM_SRC_DIR}/shaders/frag_text.glsl frag_text.glsl.h)
add_asset(${CMAKE_CURRENT_LIST_DIR}/assets/opensans.ttf opensans.ttf.h)

set(SOURCES_LIST
    ${PROGRAM_SRC_DIR}/application.cpp
    ${PROGRAM_SRC_DIR}/hexview.cpp
    ${PROGRAM_SRC_DIR}/glview.cpp
    ${PROGRAM_SRC_DIR}/textureview.cpp
    ${PROGRAM_SRC_DIR}/resourcestore.cpp
    ${PROGRAM_SRC_DIR}/modelview.cpp
    ${PROGRAM_SRC_DIR}/modelconv.cpp
    ${PROGRAM_SRC_DIR}/modeldisplay.cpp
    ${PROGRAM_SRC_DIR}/constsplitter.cpp
    ${PROGRAM_SRC_DIR}/common/binaryreader.cpp
    ${PROGRAM_SRC_DIR}/common/byteutils.cpp
    ${PROGRAM_SRC_DIR}/formats/act.cpp
    ${PROGRAM_SRC_DIR}/formats/gmo.cpp
    ${PROGRAM_SRC_DIR}/formats/gxt.cpp
    ${PROGRAM_SRC_DIR}/formats/gxp.cpp
    ${PROGRAM_SRC_DIR}/formats/gxx.cpp
    ${PROGRAM_SRC_DIR}/graphics/gl.cpp
    ${PROGRAM_SRC_DIR}/graphics/shader.cpp
    ${PROGRAM_SRC_DIR}/graphics/buffer.cpp
    ${PROGRAM_SRC_DIR}/graphics/texture.cpp
    ${PROGRAM_SRC_DIR}/graphics/framebuffer.cpp
    ${PROGRAM_SRC_DIR}/render/model.cpp
    ${PROGRAM_SRC_DIR}/render/text.cpp
    ${PROGRAM_SRC_DIR}/render/device.cpp
    ${PROGRAM_SRC_DIR}/render/camera.cpp
)

if(WIN32)
    add_executable(${PROJECT_NAME} WIN32 ${SOURCES_LIST} ${GENERATED_HEADERS})
else()
    add_executable(${PROJECT_NAME} ${SOURCES_LIST} ${GENERATED_HEADERS})
endif()

if(WIN32)
    if(MSVC)
        set(SUBSYSTEM_LINKER_OPTIONS 
            /SUBSYSTEM:WINDOWS
            /ENTRY:mainCRTStartup
        )
    else()
        set(SUBSYSTEM_LINKER_OPTIONS
            -Wl,/subsystem:windows
            -Wl,/entry:mainCRTStartup
        )
    endif()
endif()

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
    target_compile_options(thirdparty PRIVATE /W0)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE 
        -Wall 
        -Wextra 
        -Wpedantic
        -Wno-missing-designated-field-initializers
    )
    target_compile_options(thirdparty PRIVATE -w)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:thirdparty> $<TARGET_OBJECTS:glad> fmt glm wx::core wx::base wx::stc wx::gl)