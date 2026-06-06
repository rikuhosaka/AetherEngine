project(Runtime)

# .cpp と .h ファイルをすべて収集（再帰的に）
file(GLOB_RECURSE Runtime_SRC
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${Runtime_SRC})

add_executable(Runtime WIN32 ${Runtime_SRC})

if (MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  # ↑ Debugなら "MultiThreadedDebugDLL" (/MDd)、それ以外は "MultiThreadedDLL" (/MD)
endif()

target_include_directories(Runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(Runtime
PUBLIC
    Engine
    Game
)

# WinMain用に WindowsSubsystem の設定（必要であれば）
if (WIN32)
    set_target_properties(Runtime PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:WINDOWS"
    )
endif()

if (MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  # ↑ Debugなら "MultiThreadedDebugDLL" (/MDd)、それ以外は "MultiThreadedDLL" (/MD)
endif()

target_precompile_headers(Runtime PRIVATE "${CMAKE_SOURCE_DIR}/Engine/Core/pch.h")

if(AETHER_PRECOMPILED_SHADERS_ONLY)
    target_compile_definitions(Runtime PRIVATE AETHER_PRECOMPILED_SHADERS_ONLY)
endif()

add_dependencies(Runtime CompileShaders)

if(AETHER_COMPILED_SHADERS)
    foreach(SHADER_CSO ${AETHER_COMPILED_SHADERS})
        get_filename_component(SHADER_FILE_NAME "${SHADER_CSO}" NAME)
        add_custom_command(
            TARGET Runtime
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:Runtime>/CompiledShaders"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SHADER_CSO}"
                "$<TARGET_FILE_DIR:Runtime>/CompiledShaders/${SHADER_FILE_NAME}"
            COMMENT "Copy ${SHADER_FILE_NAME} to Runtime output directory")
    endforeach()
endif()

if (EXISTS "${CMAKE_SOURCE_DIR}/dx12_debug.json")
    add_custom_command(
        TARGET Runtime
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/dx12_debug.json"
            "$<TARGET_FILE_DIR:Runtime>/dx12_debug.json"
        COMMENT "Copy dx12_debug.json to Runtime output directory")
endif()

if(AETHER_ENABLE_PIX)
    add_custom_command(
        TARGET Runtime
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:WinPixEventRuntime>"
            "$<TARGET_FILE_DIR:Runtime>"
        COMMENT "Copy WinPixEventRuntime.dll next to Runtime.exe")
endif()