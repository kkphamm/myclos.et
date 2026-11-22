#pragma once
#include "avatar.hpp"

namespace closet {
struct Tape {
    float y, cm;
};

struct BodyMeasurements {
    Tape chest, waist, hips;
    float sole, crown, chin, crotch, shoulderWidthCm;
};

float tapeCircumference(const Mesh& mesh,const AvatarRest& rest,float y);
BodyMeasurements measureBody(const Mesh& mesh,bool masculine);
}
