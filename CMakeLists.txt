cmake_minimum_required(VERSION 3.16)
project(video_pipeline_vulkan)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)
find_program(GLSLANG_VALIDATOR glslangValidator REQUIRED)

# --- shaders: .comp -> .spv, rebuilt whenever the source changes ------------
set(SHADER_SOURCES
    shaders/grayscale.comp
)

set(SPIRV_BINARIES "")
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME_WE)
    set(SPIRV ${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.spv)
    add_custom_command(
        OUTPUT  ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
        COMMAND ${GLSLANG_VALIDATOR} -V ${CMAKE_SOURCE_DIR}/${SHADER} -o ${SPIRV}
        DEPENDS ${CMAKE_SOURCE_DIR}/${SHADER}
        COMMENT "Compiling ${SHADER}"
        VERBATIM
    )
    list(APPEND SPIRV_BINARIES ${SPIRV})
endforeach()

add_custom_target(shaders ALL DEPENDS ${SPIRV_BINARIES})

# --- executable -------------------------------------------------------------
add_executable(vpv
    src/main.cpp
    src/vk_buffer.cpp
    src/vk_context.cpp
    src/vk_pipeline.cpp
    src/hex_io.cpp
    src/bench.cpp
)

add_dependencies(vpv shaders)

target_include_directories(vpv PRIVATE include)
target_compile_options(vpv PRIVATE -Wall -Wextra)
target_compile_definitions(vpv PRIVATE
    SHADER_DIR="${CMAKE_BINARY_DIR}/shaders"
    DATA_DIR="${CMAKE_SOURCE_DIR}/data"
)
target_link_libraries(vpv PRIVATE Vulkan::Vulkan)
