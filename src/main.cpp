#include "viewer/fitting_room.hpp"
#include "viewer/capture.hpp"
#include "ui/ui.hpp"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc,char** argv) {
    std::filesystem::path captures;
    const bool heroes=argc==3 && std::string(argv[1])=="--hero-report";
    const bool fitReport=heroes || (argc==3 && std::string(argv[1])=="--fit-report");
    if(fitReport || (argc==3 && std::string(argv[1])=="--capture")) captures=argv[2];
    else if(argc>1 && std::string(argv[1]).rfind("-psn_",0)!=0){
        std::cerr<<"Usage: mycloset [--capture DIRECTORY | --hero-report DIRECTORY | --fit-report DIRECTORY]\n";return 2;
    }
    glfwSetErrorCallback([](int,const char* error){std::cerr<<error<<'\n';});
    if(!captures.empty()) glfwInitHint(GLFW_COCOA_MENUBAR,GLFW_FALSE);
    if(!glfwInit()) return 1;
    if(!captures.empty()) glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,GLFW_TRUE);
    auto window=glfwCreateWindow(1280,800,"myclos.et",nullptr,nullptr);
    if(!window){glfwTerminate();return 1;}
    glfwSetWindowSizeLimits(window,1000,660,GLFW_DONT_CARE,GLFW_DONT_CARE);
    glfwMakeContextCurrent(window);glfwSwapInterval(1);
    IMGUI_CHECKVERSION();ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::GetIO().IniFilename=nullptr;
    closet::initStyle();
    ImGui_ImplGlfw_InitForOpenGL(window,true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
    int status=0;
    try{
        if(fitReport) closet::captureFitReport(captures,heroes);
        else closet::runFittingRoom(window,captures);
    }
    catch(const std::exception& e){std::cerr<<"myclos.et: "<<e.what()<<'\n';status=1;}
    ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();
    glfwDestroyWindow(window);glfwTerminate();return status;
}
