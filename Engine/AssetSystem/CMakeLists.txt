file(GLOB_RECURSE AssetSystem_SRC
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${AssetSystem_SRC})

add_library(AssetSystem STATIC ${AssetSystem_SRC})

if (MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

target_include_directories(AssetSystem
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(AssetSystem
    PUBLIC
        Core
        Math
)

target_compile_definitions(AssetSystem PRIVATE NOMINMAX)

target_precompile_headers(AssetSystem PRIVATE "${CMAKE_SOURCE_DIR}/Engine/Core/pch.h")
set_target_properties(AssetSystem PROPERTIES FOLDER "Engine/AssetSystem")
