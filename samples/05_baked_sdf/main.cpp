// サンプル 05: SDF を焼いて描く。 サンプルを作って SampleApp に渡すだけ
#include "BakedSdfSample.h"

#include "common/SampleApp.h"

#include <exception>

#ifndef SAMPLE_SHADER_DIR
#define SAMPLE_SHADER_DIR "shaders"
#endif

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int showCommand)
{
    try {
        BakedSdfSample sample(L"" SAMPLE_SHADER_DIR);
        SampleApp app(instance, L"05 SDF を焼いて描く");
        return app.run(sample, showCommand);
    } catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Sample 05", MB_ICONERROR);
        return 1;
    }
}
