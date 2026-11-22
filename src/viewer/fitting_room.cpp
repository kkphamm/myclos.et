#include "fitting_room.hpp"
#include "capture.hpp"
#include "renderer.hpp"
#include "simulation/avatar.hpp"
#include "simulation/run.hpp"
#include "ui/ui.hpp"
#include <GLFW/glfw3.h>
#include <OpenGL/gl3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <future>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <utility>

namespace closet {
namespace {
using Clock=std::chrono::steady_clock;

Simulation drape(Body body,int item,int size,bool masculine,const BodyContact& contact) {
    Simulation sim(body,item,size,masculine,contact);
    bool settled;
    settle(sim,settled);
    if(!finiteState(sim)) throw std::runtime_error("Invalid preview simulation");
    if(!settled) std::cerr<<items[item].name<<" reached its preview settling time limit\n";
    return sim;
}

struct Entry {
    Target target;
    std::optional<Simulation> sim;
};
} // namespace

void runFittingRoom(GLFWwindow* window,const std::filesystem::path& captures) {
    Renderer renderer;
    Ui ui;
    auto avatar=bodyMesh(ui.body,ui.masculine);
    auto contact=fittingContact(avatar,ui.masculine);
    auto display=displayBodyMesh(avatar,ui.masculine,ui.body.height);
    std::array<std::optional<Simulation>,2> outfit;
    DrapeKey model{ui.body,0,ui.masculine};
    ui.reset=true;
    Target scene;
    std::array<Entry,items.size()> cards;
    auto previewKey=[&](int item){ return DrapeKey{ui.body,ui.garmentSizes[items[item].bottom()],ui.masculine}; };
    std::future<Simulation> job;
    double previousTime=glfwGetTime(), accumulator=0;
    auto storeStart=Clock::now();
    bool timingStore=false, previousStore=false;
    CaptureReplay capture(captures);

    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if(glfwGetWindowAttrib(window,GLFW_ICONIFIED)){
            glfwWaitEventsTimeout(.10); previousTime=glfwGetTime(); continue;
        }
        capture.applyControls(ui);

        const bool reset=ui.reset;
        ui.reset=false;
        const DrapeKey wantedModel{ui.body,0,ui.masculine};
        const bool modelChanged=ui.masculine!=model.masculine;
        const bool formChanged=!(wantedModel==model);
        if(formChanged){
            avatar=bodyMesh(ui.body,ui.masculine);
            contact=fittingContact(avatar,ui.masculine);
            display=displayBodyMesh(avatar,ui.masculine,ui.body.height);
            model=wantedModel;
        }
        auto matchesPreview=[&](const Simulation& sim){
            return DrapeKey{sim.body,sim.size,sim.masculine}==previewKey(sim.item);
        };
        for(auto& card:cards) if(card.sim && !matchesPreview(*card.sim)){
            card.sim.reset();
            renderer.release(card.target);
            timingStore=false;
        }
        if(job.valid() && job.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
            auto result=job.get();
            if(matchesPreview(result)) cards[result.item].sim.emplace(std::move(result));
        }

        bool changed=formChanged || reset;
        for(int slot=0;slot<2;++slot) {
            auto& sim=outfit[slot];
            const int item=ui.worn[slot],size=ui.garmentSizes[slot];
            if(item<0) {
                sim.reset();
            } else if(reset || !sim || sim->item!=item || sim->size!=size || modelChanged) {
                if(cards[item].sim) sim=cards[item].sim;
                else if(reset) sim=drape(ui.body,item,size,ui.masculine,contact);
                else sim.emplace(ui.body,item,size,ui.masculine,contact);
                changed=true;
            } else if(formChanged){
                sim->updateBody(ui.body,contact);
            }
        }
        if(changed && outfit[0] && outfit[1]) layerOutfit(*outfit[0],*outfit[1]);

        std::array<unsigned,items.size()> thumbs{};
        int ready=0, missing=-1;
        int visible=0;
        for(size_t i=0;i<cards.size();++i) {
            if(items[i].mens!=ui.mensCollection) continue;
            ++visible;
            auto& card=cards[i];
            if(card.sim){
                if(ui.store && !card.target.texture){
                    Camera camera{};
                    camera.productView=true;
                    renderer.draw(card.target,880,500,display,ui.body.height,&*card.sim,nullptr,camera,false);
                }
                thumbs[i]=card.target.texture;
                ++ready;
            } else if(missing<0) missing=int(i);
        }
        if(ui.store && !previousStore && ready==visible) std::cout<<"Reused collection previews"<<std::endl;
        previousStore=ui.store;
        if(ui.store && missing>=0 && !job.valid() && !ImGui::GetIO().MouseDown[0]){
            if(!timingStore){storeStart=Clock::now(); timingStore=true;}
            job=std::async(std::launch::async,drape,ui.body,missing,previewKey(missing).size,ui.masculine,contact);
        }
        if(timingStore && ready==visible){
            std::cout<<"Store ready in "<<std::chrono::duration<double>(Clock::now()-storeStart).count()<<" s"<<std::endl;
            timingStore=false;
        }

        double now=glfwGetTime();
        accumulator=std::min(.065,accumulator+std::max(0.,now-previousTime));
        previousTime=now;
        if(ui.store) accumulator=0;
        while(accumulator>=stepDt){
            for(auto& sim:outfit) if(sim) sim->step(stepDt,ui.wind?ui.strength:0,ui.direction);
            if(outfit[0] && outfit[1]) layerOutfit(*outfit[0],*outfit[1]);
            accumulator-=stepDt;
        }

        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
        const auto& io=ImGui::GetIO();
        if(!ui.store && io.DisplaySize.x>0 && io.DisplaySize.y>0){
            auto size=sceneSize(io.DisplaySize.x,io.DisplaySize.y);
            auto layer=[&](int slot) -> const Simulation* { return outfit[slot] ? &*outfit[slot] : nullptr; };
            renderer.draw(scene,std::max(1,int(size[0]*io.DisplayFramebufferScale.x)),
                           std::max(1,int(size[1]*io.DisplayFramebufferScale.y)),
                           display,ui.body.height,layer(ui.slot),layer(1-ui.slot),{ui.yaw,ui.pitch,ui.zoom},ui.heatmap);
        }
        drawUi(ui,scene.texture,thumbs);
        ImGui::Render();
        int w,h;glfwGetFramebufferSize(window,&w,&h);
        glBindFramebuffer(GL_FRAMEBUFFER,0);
        glViewport(0,0,w,h); glClearColor(.95f,.94f,.92f,1);glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if(capture.saveIfReady(!ui.store || ready==visible,w,h))
            glfwSetWindowShouldClose(window,GLFW_TRUE);
        glfwSwapBuffers(window);
    }
    for(auto& c:cards) renderer.release(c.target);
    renderer.release(scene);
}
} // namespace closet
