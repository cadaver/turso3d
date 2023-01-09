# For conditions of distribution and use, see copyright notice in License.txt

set (TARGET_NAME Turso3D)

# Macro to add files from a directory grouped to the solution file
macro (add_engine_directory DIRECTORY)
    file (GLOB DIR_FILES ${DIRECTORY}/*.h ${DIRECTORY}/*.c ${DIRECTORY}/*.cpp)
    get_filename_component (GROUP ${DIRECTORY} NAME)
    source_group ("${GROUP}" FILES ${DIR_FILES})
    list (APPEND SOURCE_FILES ${DIR_FILES})
endmacro ()

macro (add_engine_directory_group DIRECTORY GROUP)
    file (GLOB DIR_FILES ${DIRECTORY}/*.h ${DIRECTORY}/*.c ${DIRECTORY}/*.cpp)
    source_group ("${GROUP}" FILES ${DIR_FILES})
    list (APPEND SOURCE_FILES ${DIR_FILES})
endmacro ()

add_engine_directory (Graphics)
add_engine_directory (../ThirdParty/FlextGL)
add_engine_directory (Input)
add_engine_directory (IO)
add_engine_directory (Math)
add_engine_directory (Object)
add_engine_directory (Renderer)
add_engine_directory (Resource)
add_engine_directory (Scene)
add_engine_directory (Thread)
add_engine_directory_group (. Main)

add_executable (${TARGET_NAME} ${SOURCE_FILES})

include_directories (../ThirdParty/SDL/include)
include_directories (../ThirdParty/GLEW)
include_directories (../ThirdParty/STB)

target_link_libraries (${TARGET_NAME} SDL2-static GLEW)

add_definitions (-DGLEW_STATIC -DSDL_MAIN_HANDLED)

if (WIN32)
    target_link_libraries (${TARGET_NAME} winmm imm32 ole32 oleaut32 setupapi version uuid opengl32)
elseif (APPLE)
    target_link_libraries (${TARGET_NAME} "-framework Carbon" "-framework Cocoa" "-framework OpenGL")
else ()
    target_link_libraries (${TARGET_NAME} -lGL -lpthread)
endif ()
