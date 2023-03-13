set (TARGET_NAME Tracy)

file (GLOB SOURCE_FILES *.cpp)

if (TURSO3D_TRACY)
    add_definitions (-DTRACY_ENABLE)
endif ()

add_library (${TARGET_NAME} ${SOURCE_FILES})
target_include_directories (${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_property(TARGET ${TARGET_NAME} PROPERTY FOLDER "ThirdParty")