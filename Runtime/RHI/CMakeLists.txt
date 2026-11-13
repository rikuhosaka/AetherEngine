add_library(RHI INTERFACE)

add_subdirectory(Common)

add_subdirectory(Interface)

add_subdirectory(DX12)

target_include_directories(RHI INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
