#pragma once
#include "closet.hpp"

namespace closet {
struct AvatarRest {
    Mesh mesh;
    std::vector<std::array<float,2>> limbWeights; // Arm and leg influences at limb junctions.
};
const AvatarRest& avatarRest(bool masculine);
inline BodyContact fittingContact(const Mesh& mesh,bool masculine) {
    return BodyContact(mesh,&avatarRest(masculine).limbWeights);
}
}
