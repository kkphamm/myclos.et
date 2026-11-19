#pragma once
#include "simulation/catalog.hpp"

namespace closet {
struct Ui {
    Body body{};
    // Slot 0 holds a top, slot 1 a bottom. A negative item leaves the slot empty.
    std::array<int,2> worn{0,2}, garmentSizes{1,1};
    int slot=0;
    int item() const { return worn[slot]; }
    int size() const { return garmentSizes[slot]; }
    void wear(int index) {
        slot=items.at(index).bottom() ? 1 : 0;
        worn[slot]=index;
    }
    void remove(int which) {
        worn.at(which)=-1;
        slot=worn[1-which]>=0 ? 1-which : which;
    }
    bool store=false, heatmap=false, wind=false;
    bool mensCollection=false, masculine=false, inches=false;
    float strength=1, direction=0, yaw=.30f, pitch=.025f, zoom=1;
    const std::array<Body,3>& modelPresets() const { return masculine?masculinePresets:presets; }
    void setModel(bool male) {
        const auto& current=modelPresets();
        for(size_t i=0;i<current.size();++i) if(body==current[i]) {
            body=(male?masculinePresets:presets)[i];
            break;
        }
        masculine=male;
    }
    bool reset=false;
};
void initStyle();
std::array<int,2> sceneSize(float displayWidth, float displayHeight);
void drawUi(Ui&, unsigned sceneTexture, const std::array<unsigned,items.size()>& thumbs);
}
