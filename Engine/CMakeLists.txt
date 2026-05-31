
add_library(Engine INTERFACE)

add_subdirectory(Core)
set_target_properties(Core PROPERTIES FOLDER "Engine")

add_subdirectory(Frame)
set_target_properties(Frame PROPERTIES FOLDER "Engine/Frame")

add_subdirectory(Graphics)
set_target_properties(Graphics PROPERTIES FOLDER "Engine/Graphics")

add_subdirectory(RHI)
set_target_properties(RHI PROPERTIES FOLDER "Engine")

add_subdirectory(Renderer)
set_target_properties(Renderer PROPERTIES FOLDER "Engine")

add_subdirectory(Platform)
set_target_properties(Platform PROPERTIES FOLDER "Engine")

add_subdirectory(Application)
set_target_properties(Application PROPERTIES FOLDER "Engine/Application")



target_link_libraries(Engine INTERFACE
    Core
    Frame
    Graphics
    RHI
    RHI_Interface
    DX12
    Renderer
    DirectXTex
    Platform
    Application
)

target_include_directories(Engine INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

