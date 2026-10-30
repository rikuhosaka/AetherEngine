file(GLOB_RECURSE EngineModule_SRC
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${EngineModule_SRC})

add_library(EngineModule STATIC ${EngineModule_SRC})

if (MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

target_include_directories(EngineModule
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(EngineModule
    PUBLIC
        World
)

target_compile_definitions(EngineModule PRIVATE NOMINMAX)

target_precompile_headers(EngineModule PRIVATE "${CMAKE_SOURCE_DIR}/Engine/Core/pch.h")
set_target_properties(EngineModule PROPERTIES FOLDER "Engine/Engine")
