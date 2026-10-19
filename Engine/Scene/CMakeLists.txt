file(GLOB_RECURSE Scene_SRC
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${Scene_SRC})

add_library(Scene STATIC ${Scene_SRC})

if (MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

target_include_directories(Scene
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(Scene
    PUBLIC
        Application
        Camera
        World
        Renderer
)

target_compile_definitions(Scene PRIVATE NOMINMAX)

target_precompile_headers(Scene PRIVATE "${CMAKE_SOURCE_DIR}/Engine/Core/pch.h")
set_target_properties(Scene PROPERTIES FOLDER "Engine/Scene")
