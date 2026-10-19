file(GLOB_RECURSE EDITOR_SRC
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${EDITOR_SRC})

add_library(Editor STATIC ${EDITOR_SRC})

if (MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

target_include_directories(Editor
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(Editor
    PUBLIC
        Engine
)

target_compile_definitions(Editor PRIVATE NOMINMAX)

target_precompile_headers(Editor PRIVATE "${CMAKE_SOURCE_DIR}/Engine/Core/pch.h")
set_target_properties(Editor PROPERTIES FOLDER "Editor")
