file(GLOB_RECURSE Graphics_SRC
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${Graphics_SRC})

add_library(Graphics STATIC ${Graphics_SRC})

target_include_directories(Graphics
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(Graphics
    PUBLIC
        Frame
        RHI
        RHI_Interface
        DX12
)

target_precompile_headers(Graphics PRIVATE "${CMAKE_SOURCE_DIR}/Engine/Core/pch.h")
set_target_properties(Graphics PROPERTIES FOLDER "Engine/Graphics")
