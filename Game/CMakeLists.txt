
# .cpp と .h ファイルをすべて収集（再帰的に）
file(GLOB_RECURSE GAME_SRC
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${GAME_SRC})

add_library(Game STATIC ${GAME_SRC})

if (MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  # ↑ Debugなら "MultiThreadedDebugDLL" (/MDd)、それ以外は "MultiThreadedDLL" (/MD)
endif()

target_include_directories(Game
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(Game PRIVATE Engine)


target_precompile_headers(Game PRIVATE "${CMAKE_SOURCE_DIR}/Engine/Core/pch.h")