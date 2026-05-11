
# FBXSDK 本体
add_library(Fbxsdk STATIC IMPORTED GLOBAL)
set_target_properties(Fbxsdk PROPERTIES
    IMPORTED_CONFIGURATIONS "Debug;Release"
    IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/lib/Debug/libfbxsdk-md.lib"
    #IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/lib/Release/libfbxsdk-md.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# libxml2
add_library(libxml2 STATIC IMPORTED GLOBAL)
set_target_properties(libxml2 PROPERTIES
    IMPORTED_CONFIGURATIONS "Debug;Release"
    IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/lib/Debug/libxml2-md.lib"
    #IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/lib/Release/libxml2-md.lib"
)

# zlib
add_library(zlib STATIC IMPORTED GLOBAL)
set_target_properties(zlib PROPERTIES
    IMPORTED_CONFIGURATIONS "Debug;Release"
    IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/lib/Debug/zlib-md.lib"
    #IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/lib/Release/zlib-md.lib"
)

# Fbxsdk にリンク依存を追加
target_link_libraries(Fbxsdk INTERFACE libxml2 zlib)

# IDE上のグループ分け（オプション）
set_target_properties(Fbxsdk libxml2 zlib PROPERTIES FOLDER "ThirdParty/Fbxsdk")
