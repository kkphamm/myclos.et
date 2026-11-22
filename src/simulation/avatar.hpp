#pragma once
#include "closet.hpp"

namespace closet {
constexpr int chinVertex=5211,shoulderVertex=8275;
struct AvatarRest {
    Mesh mesh;
    std::vector<std::array<float,2>> limbWeights; // Arm and leg influences at limb junctions.
};
const AvatarRest& avatarRest(bool masculine);
Mesh displayBodyMesh(const Mesh& mesh,bool masculine,float heightCm);
BodyContact fittingContact(const Mesh& mesh,bool masculine);
}
