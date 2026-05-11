
# FBXSDK 本体
add_library(Jolt STATIC IMPORTED GLOBAL)
set_target_properties(Jolt PROPERTIES
    IMPORTED_CONFIGURATIONS "Debug;Release"
    IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/lib/Debug/jolt.lib"
    IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/lib/Release/jolt.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# IDE上のグループ分け（オプション）
set_target_properties(Jolt PROPERTIES FOLDER "ThirdParty/Jolt")