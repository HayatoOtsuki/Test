// サンプル 02: SDF だけ。 サンプルを作って SampleApp に渡すだけ
#include "SdfSliceSample.h"

#include "common/SampleApp.h"

#include <exception>

#ifndef SAMPLE_SHADER_DIR
#define SAMPLE_SHADER_DIR "shaders"
#endif

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int showCommand)
{
    try {
        SdfSliceSample sample(L"" SAMPLE_SHADER_DIR);
        SampleApp app(instance, L"02 SDF だけ");
        return app.run(sample, showCommand);
    } catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Sample 02", MB_ICONERROR);
        return 1;
    }
}
