add_library(Frame INTERFACE)
target_include_directories(Frame INTERFACE ${CMAKE_SOURCE_DIR})
set_target_properties(Frame PROPERTIES FOLDER "Engine/Frame")
