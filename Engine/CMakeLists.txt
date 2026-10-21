
add_library(Engine INTERFACE)

add_subdirectory(Core)
set_target_properties(Core PROPERTIES FOLDER "Engine")

add_subdirectory(Math)
set_target_properties(Math PROPERTIES FOLDER "Engine/Math")

add_subdirectory(Frame)
set_target_properties(Frame PROPERTIES FOLDER "Engine/Frame")

add_subdirectory(Graphics)
set_target_properties(Graphics PROPERTIES FOLDER "Engine/Graphics")

add_subdirectory(Platform)
set_target_properties(Platform PROPERTIES FOLDER "Engine")

add_subdirectory(RHI)
set_target_properties(RHI PROPERTIES FOLDER "Engine")

add_subdirectory(AssetSystem)
set_target_properties(AssetSystem PROPERTIES FOLDER "Engine/AssetSystem")

add_subdirectory(Renderer)
set_target_properties(Renderer PROPERTIES FOLDER "Engine")

add_subdirectory(Camera)
set_target_properties(Camera PROPERTIES FOLDER "Engine/Camera")

add_subdirectory(Application)
set_target_properties(Application PROPERTIES FOLDER "Engine/Application")

add_subdirectory(Physics)
set_target_properties(Physics PROPERTIES FOLDER "Engine/Physics")

add_subdirectory(World)
set_target_properties(World PROPERTIES FOLDER "Engine/World")

add_subdirectory(Scene)
set_target_properties(Scene PROPERTIES FOLDER "Engine/Scene")

target_link_libraries(Engine INTERFACE
    Core
    Math
    Frame
    Graphics
    RHI
    RHI_Interface
    Renderer
    Camera
    DirectXTex
    Platform
    Application
    World
    Physics
    AssetSystem
    Scene
)

target_include_directories(Engine INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

