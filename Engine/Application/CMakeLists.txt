file(GLOB_RECURSE Application_SRC
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${Application_SRC})

add_library(Application STATIC ${Application_SRC})

target_include_directories(Application
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(Application
    PUBLIC
        Core
        Frame
        RHI_Interface
)

target_precompile_headers(Application PRIVATE "${CMAKE_SOURCE_DIR}/Engine/Core/pch.h")
set_target_properties(Application PROPERTIES FOLDER "Engine/Application")
