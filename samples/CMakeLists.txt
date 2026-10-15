# 機能ごとの簡易サンプル。 1 つのサンプル = 1 つの exe (Sample01Primitives など)

# サンプルの共通の土台 (窓・カメラ・ライトの回し方、 全画面のパス、 GPU の小さな道具)
add_library(SampleCommon STATIC
    common/SampleApp.cpp      common/SampleApp.h
    common/SampleEntity.h
    common/FullscreenPass.cpp common/FullscreenPass.h
    common/GpuHelpers.cpp     common/GpuHelpers.h
)
# #include "common/..." を samples から探す
target_include_directories(SampleCommon PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(SampleCommon PUBLIC SdfEngine)
# 背景 (空と床) のシェーダーはエンジンの物を使う
target_compile_definitions(SampleCommon PRIVATE "ENGINE_SHADER_DIR=\"${PROJECT_SOURCE_DIR}/src/engine/shaders\"")
set_target_properties(SampleCommon PROPERTIES FOLDER samples)

# サンプルを 1 つ足す: add_sample(exe の名前 フォルダ ソース...)
# シェーダーは実行時に <フォルダ>/shaders から読む (R キーで読み直せる)
function(add_sample target dir)
    add_executable(${target} WIN32 ${ARGN})
    target_link_libraries(${target} PRIVATE SampleCommon)
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/${dir})
    target_compile_definitions(${target} PRIVATE "SAMPLE_SHADER_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/${dir}/shaders\"")
    set_target_properties(${target} PROPERTIES FOLDER samples)
endfunction()

# ---- サンプル ----
add_sample(Sample01Primitives 01_primitives
    01_primitives/main.cpp
    01_primitives/PrimitiveSample.cpp 01_primitives/PrimitiveSample.h
    01_primitives/PrimitiveMesh.cpp   01_primitives/PrimitiveMesh.h
)
add_sample(Sample02Sdf 02_sdf
    02_sdf/main.cpp
    02_sdf/SdfSliceSample.cpp 02_sdf/SdfSliceSample.h
)
add_sample(Sample03Raymarch 03_raymarch
    03_raymarch/main.cpp
    03_raymarch/RaymarchSample.cpp 03_raymarch/RaymarchSample.h
)
add_sample(Sample04SdfRaymarch 04_sdf_raymarch
    04_sdf_raymarch/main.cpp
    04_sdf_raymarch/SdfRaymarchSample.cpp 04_sdf_raymarch/SdfRaymarchSample.h
)
