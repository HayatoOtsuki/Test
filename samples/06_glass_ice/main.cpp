// サンプル 06: コップの氷。 サンプルを作って SampleApp に渡すだけ
#include "GlassIceSample.h"

#include "common/SampleApp.h"

#include <exception>

#ifndef SAMPLE_SHADER_DIR
#define SAMPLE_SHADER_DIR "shaders"
#endif

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int showCommand)
{
    try {
        GlassIceSample sample(L"" SAMPLE_SHADER_DIR);
        SampleApp app(instance, L"06 コップの氷");
        return app.run(sample, showCommand);
    } catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Sample 06", MB_ICONERROR);
        return 1;
    }
}
