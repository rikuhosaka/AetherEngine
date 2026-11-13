
add_library(Engine INTERFACE)

add_subdirectory(Camera)
set_target_properties(Camera PROPERTIES FOLDER "Runtime/Engine/Camera")

add_subdirectory(Application)
set_target_properties(Application PROPERTIES FOLDER "Runtime/Engine/Application")

add_subdirectory(Physics)
set_target_properties(Physics PROPERTIES FOLDER "Runtime/Engine/Physics")

add_subdirectory(World)
set_target_properties(World PROPERTIES FOLDER "Runtime/Engine/World")

add_subdirectory(SceneDocument)
set_target_properties(SceneDocument PROPERTIES FOLDER "Runtime/Engine/SceneDocument")

# EngineModule links Editor later. Keep it out of this interface so Editor can keep linking Engine.
add_subdirectory(Engine)
set_target_properties(EngineModule PROPERTIES FOLDER "Runtime/Engine/Engine")

target_link_libraries(Engine INTERFACE
    Core
    Math
    Camera
    DirectXTex
    Platform
    Application
    World
    Physics
    AssetSystem
    SceneDocument
)

target_include_directories(Engine INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

