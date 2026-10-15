// サンプル 03: レイマーチングだけ。 サンプルを作って SampleApp に渡すだけ
#include "RaymarchSample.h"

#include "common/SampleApp.h"

#include <exception>

#ifndef SAMPLE_SHADER_DIR
#define SAMPLE_SHADER_DIR "shaders"
#endif

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int showCommand)
{
    try {
        RaymarchSample sample(L"" SAMPLE_SHADER_DIR);
        SampleApp app(instance, L"03 レイマーチングだけ");
        return app.run(sample, showCommand);
    } catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Sample 03", MB_ICONERROR);
        return 1;
    }
}
