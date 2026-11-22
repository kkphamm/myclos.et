#pragma once
#include <filesystem>

namespace closet {
struct Ui;
void captureFitReport(const std::filesystem::path& directory,bool heroes);

class CaptureReplay {
public:
    explicit CaptureReplay(const std::filesystem::path& directory);
    void applyControls(Ui& ui) const;
    bool saveIfReady(bool previewsReady,int width,int height);
private:
    std::filesystem::path directory;
    int stage=0, renderedFrames=0;
};
}
