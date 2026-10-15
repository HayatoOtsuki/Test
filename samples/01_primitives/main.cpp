// サンプル 01: プリミティブだけ。 サンプルを作って SampleApp に渡すだけ
#include "PrimitiveSample.h"

#include "common/SampleApp.h"

#include <exception>

#ifndef SAMPLE_SHADER_DIR
#define SAMPLE_SHADER_DIR "shaders"
#endif

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int showCommand)
{
    try {
        PrimitiveSample sample(L"" SAMPLE_SHADER_DIR);
        SampleApp app(instance, L"01 プリミティブだけ");
        return app.run(sample, showCommand);
    } catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Sample 01", MB_ICONERROR);
        return 1;
    }
}
