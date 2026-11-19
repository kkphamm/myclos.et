#pragma once
#include <filesystem>

struct GLFWwindow;

namespace closet {
void runFittingRoom(GLFWwindow* window,const std::filesystem::path& captures);
}
