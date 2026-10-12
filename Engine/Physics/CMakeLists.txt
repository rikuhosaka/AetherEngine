file(GLOB_RECURSE Physics_SRC
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${Physics_SRC})

add_library(Physics STATIC ${Physics_SRC})

if (MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

target_include_directories(Physics
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

# Jolt の include とコンパイル定義はこのターゲットだけが使う。
# 利用側へ渡すのはリンクだけで、公開ヘッダに Jolt 型は出さない。
target_link_libraries(Physics
    PUBLIC
        Core
        World
        $<LINK_ONLY:Jolt>
    PRIVATE
        Jolt
)

# pch が windows.h を先に取り込む。NOMINMAX はコンパイル定義として先に渡す。
target_compile_definitions(Physics PRIVATE NOMINMAX)

target_precompile_headers(Physics PRIVATE "${CMAKE_SOURCE_DIR}/Engine/Core/pch.h")
set_target_properties(Physics PROPERTIES FOLDER "Engine/Physics")
