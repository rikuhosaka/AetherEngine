# 同梱の lib/Debug と lib/Release は /MTd と /MT でビルドされている。
# エンジンは /MD なので、ここではソースから Jolt を作りランタイムを揃える。
set(PHYSICS_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/include")

set(USE_SSE4_1 ON)
set(USE_SSE4_2 ON)
set(USE_AVX ON)
set(USE_AVX2 ON)
set(USE_LZCNT ON)
set(USE_TZCNT ON)
set(USE_F16C ON)
set(USE_FMADD ON)
set(ENABLE_OBJECT_STREAM ON)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

include("${PHYSICS_REPO_ROOT}/Jolt/Jolt.cmake")

set_target_properties(Jolt PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    FOLDER "ThirdParty/Jolt"
)
