# For conditions of distribution and use, see copyright notice in License.txt

set (TARGET_NAME Turso3DTest)

file (GLOB SOURCE_FILES *.h *.cpp)

add_definitions (-DGLEW_STATIC -DSDL_MAIN_HANDLED)

if (TURSO3D_TRACY)
    add_definitions (-DTRACY_ENABLE)
endif ()

add_executable (${TARGET_NAME} ${SOURCE_FILES})

target_link_libraries (${TARGET_NAME} SDL2-static Turso3D GLEW Tracy)

if (WIN32)
    target_link_libraries (${TARGET_NAME} winmm imm32 ole32 oleaut32 setupapi version uuid opengl32)
elseif (APPLE)
    target_link_libraries (${TARGET_NAME} "-framework Carbon" "-framework Cocoa" "-framework OpenGL")
else ()
    target_link_libraries (${TARGET_NAME} -lGL -lpthread)
endif ()
