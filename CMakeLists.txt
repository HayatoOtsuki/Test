cmake_minimum_required(VERSION 3.20)
project(SdfIce CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# -----------------------------------------------------------------------------
# エンジンの代わり (静的ライブラリ。 本体と簡易サンプルが使う)
# -----------------------------------------------------------------------------
set(ENGINE_SOURCES
    src/engine/Window.cpp         src/engine/Window.h
    src/engine/OrbitCamera.cpp    src/engine/OrbitCamera.h
    src/engine/GraphicsDevice.cpp src/engine/GraphicsDevice.h
    src/engine/SwapChain.cpp      src/engine/SwapChain.h
    src/engine/DepthBuffer.cpp    src/engine/DepthBuffer.h
    src/engine/Renderer.cpp       src/engine/Renderer.h
    src/engine/BackgroundPass.cpp src/engine/BackgroundPass.h
    src/engine/ShaderCompiler.cpp src/engine/ShaderCompiler.h
    src/engine/DrawContext.h
    src/engine/DirectionalLight.h
    src/engine/Material.h
    src/engine/SceneConstants.h
    src/engine/RenderEntity.h
    src/engine/MathTypes.h
    src/engine/DxCheck.h
)
add_library(SdfEngine STATIC ${ENGINE_SOURCES})
# #include "engine/..." と #include "ice/..." を src から探す
target_include_directories(SdfEngine PUBLIC src)
target_compile_definitions(SdfEngine PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(SdfEngine PUBLIC d3d12 dxgi d3dcompiler)
if(MSVC)
    # /utf-8: ソースのコメント (日本語) を UTF-8 として読む
    target_compile_options(SdfEngine PUBLIC /utf-8 /W4)
endif()
set_target_properties(SdfEngine PROPERTIES FOLDER engine)

# -----------------------------------------------------------------------------
# 本体: 氷の部品 (自分のエンジンへ移すのは src/ice) と、 それを動かすアプリ
# -----------------------------------------------------------------------------
set(ICE_SOURCES
    src/ice/IceComponent.cpp src/ice/IceComponent.h
    src/ice/IceImplement.cpp src/ice/IceImplement.h
    src/ice/IceGenerate.cpp
)
set(APP_SOURCES
    src/main.cpp
    src/engine/Application.cpp src/engine/Application.h  # 氷の部品を使うので、 エンジンのライブラリには入れない
)
add_executable(SdfIce WIN32 ${APP_SOURCES} ${ICE_SOURCES})
source_group(ice FILES ${ICE_SOURCES})
target_link_libraries(SdfIce PRIVATE SdfEngine)
# シェーダーは実行時にソースの場所から読む (R キーで読み直せる)
target_compile_definitions(SdfIce PRIVATE "SHADER_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}/src\"")
if(MSVC)
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT SdfIce)
endif()

# -----------------------------------------------------------------------------
# 機能ごとの簡易サンプル (samples/)
# -----------------------------------------------------------------------------
add_subdirectory(samples)
