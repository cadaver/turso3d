set (TARGET_NAME GLEW)

file (GLOB SOURCE_FILES *.h *.c *.cpp)

add_library (${TARGET_NAME} ${SOURCE_FILES})
target_compile_definitions (${TARGET_NAME} PUBLIC -DGLEW_STATIC -DGLEW_NO_GLU)
target_include_directories (${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set_property(TARGET ${TARGET_NAME} PROPERTY FOLDER "ThirdParty")