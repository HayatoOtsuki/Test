cmake_minimum_required(VERSION 3.20)
project(SdfIce CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# エンジンの代わり (氷の部品を動かすための土台)
set(ENGINE_SOURCES
    src/engine/Application.cpp    src/engine/Application.h
    src/engine/Window.cpp         src/engine/Window.h
    src/engine/OrbitCamera.cpp    src/engine/OrbitCamera.h
    src/engine/GraphicsDevice.cpp src/engine/GraphicsDevice.h
    src/engine/SwapChain.cpp      src/engine/SwapChain.h
    src/engine/DepthBuffer.cpp    src/engine/DepthBuffer.h
    src/engine/Renderer.cpp       src/engine/Renderer.h
    src/engine/BackgroundPass.cpp src/engine/BackgroundPass.h
    src/engine/ShaderCompiler.cpp src/engine/ShaderCompiler.h
    src/engine/DrawContext.h
    src/engine/MathTypes.h
    src/engine/DxCheck.h
)

add_executable(SdfIce WIN32 src/main.cpp ${ENGINE_SOURCES})
source_group(engine FILES ${ENGINE_SOURCES})

# #include "engine/..." と #include "ice/..." を src から探す
target_include_directories(SdfIce PRIVATE src)

# シェーダーは実行時にソースの場所から読む (R キーで読み直せる)
target_compile_definitions(SdfIce PRIVATE
    UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
    "SHADER_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}/src\"")
target_link_libraries(SdfIce PRIVATE d3d12 dxgi d3dcompiler)

if(MSVC)
    # /utf-8: ソースのコメント (日本語) を UTF-8 として読む
    target_compile_options(SdfIce PRIVATE /utf-8 /W4)
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT SdfIce)
endif()
