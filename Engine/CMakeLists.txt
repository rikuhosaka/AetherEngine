
add_library(Engine INTERFACE)

add_subdirectory(Core)
set_target_properties(Core PROPERTIES FOLDER "Engine")

add_subdirectory(Graphics)
set_target_properties(Graphics PROPERTIES FOLDER "Engine")

add_subdirectory(RHI)
set_target_properties(RHI PROPERTIES FOLDER "Engine")

add_subdirectory(Renderer)
set_target_properties(Renderer PROPERTIES FOLDER "Engine")

add_subdirectory(Platform)
set_target_properties(Platform PROPERTIES FOLDER "Engine")



target_link_libraries(Engine INTERFACE
    Core
    Graphics
    RHI
    RHI_Interface
    DX12
    Renderer
    DirectXTex
    Platform
)

target_include_directories(Engine INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

