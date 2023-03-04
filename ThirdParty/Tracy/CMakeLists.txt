set (TARGET_NAME Tracy)

file (GLOB SOURCE_FILES *.cpp)

if (TURSO3D_TRACY)
    add_definitions (-DTRACY_ENABLE)
endif ()

add_library (${TARGET_NAME} ${SOURCE_FILES})

include_directories (.)