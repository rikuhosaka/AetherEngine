file(GLOB_RECURSE SceneDocument_SRC
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SceneDocument_SRC})

add_library(SceneDocument STATIC ${SceneDocument_SRC})

if (MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

target_include_directories(SceneDocument
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(SceneDocument
    PUBLIC
        Application
        Camera
        Physics
        World
        Renderer
        AssetSystem
)

target_compile_definitions(SceneDocument PRIVATE NOMINMAX)

target_precompile_headers(SceneDocument PRIVATE "${CMAKE_SOURCE_DIR}/Engine/Core/pch.h")
set_target_properties(SceneDocument PROPERTIES FOLDER "Engine/SceneDocument")
