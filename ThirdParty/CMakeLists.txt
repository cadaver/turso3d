add_subdirectory (GLEW)
add_subdirectory (SDL EXCLUDE_FROM_ALL)
add_subdirectory (STB)
add_subdirectory (Tracy)

set_property(TARGET SDL2-static PROPERTY FOLDER "ThirdParty")