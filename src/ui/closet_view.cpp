#include "ui_internal.hpp"
#include <algorithm>
#include <cstdio>

namespace closet::ui_detail {
void store(Ui& ui,const std::array<unsigned,items.size()>& thumbs,float width,float height) {
    auto* dl=ImGui::GetWindowDrawList();
    const float margin=std::clamp(width*.031f,20.f,40.f),gutter=16,caption=55;
    const int columns=ui.mensCollection && width>=780 ? 3 : 2;
    const float maxImageH=(height-91-margin-gutter)*.5f-caption;
    const float cardW=std::min((width-2*margin-(columns-1)*gutter)/columns,maxImageH*880.f/500.f);
    const float imageH=std::min(cardW*.72f,maxImageH);
    const float left=(width-columns*cardW-(columns-1)*gutter)*.5f,right=width-left;
    label(dl,{left,header+13},serif,38,ink,"The closet");
    ImGui::SetCursorScreenPos({right-148,header+25});
    ImGui::PushID("collection");
    if(choice("Women",!ui.mensCollection,77)) ui.mensCollection=false;
    ImGui::SameLine(0,4);
    if(choice("Men",ui.mensCollection,67)) ui.mensCollection=true;
    ImGui::PopID();
    int count=0,ready=0;
    for(size_t i=0;i<items.size();++i) if(items[i].mens==ui.mensCollection) {
        ++count; if(thumbs[i]) ++ready;
    }
    char status[80];
    if(ready<count) std::snprintf(status,sizeof(status),"Draping previews  ·  %d of %d ready",ready,count);
    else std::snprintf(status,sizeof(status),"%s  ·  %d styles  ·  Tops %s / Bottoms %s",ui.mensCollection?"Menswear":"Womenswear",count,
                       sizes[ui.garmentSizes[0]],sizes[ui.garmentSizes[1]]);
    label(dl,{left+1,header+60},sans,14,muted,status);
    ImGui::SetCursorScreenPos({0,header+91});
    ImGui::BeginChild(ui.mensCollection?"Menswear gallery":"Womenswear gallery",{width,height-91});
    dl=ImGui::GetWindowDrawList();
    const float top=ImGui::GetCursorScreenPos().y;
    int slot=0;
    for(size_t i=0;i<items.size();++i) {
        if(items[i].mens!=ui.mensCollection) continue;
        const float x=left+(slot%columns)*(cardW+gutter),y=top+(slot/columns)*(imageH+caption+gutter);
        ++slot;
        const ImVec2 p(x,y), imageEnd(x+cardW,y+imageH),end(x+cardW,imageEnd.y+caption);
        const bool available=thumbs[i]!=0;
        if(available) {
            // Crop the wider virtual contact sheet to the card without changing the garment's proportions.
            float visibleX=std::min(1.f,(cardW/imageH)/(880.f/500.f));
            dl->AddImageRounded(thumbs[i],p,imageEnd,{(1-visibleX)*.5f,1},{(1+visibleX)*.5f,0},white,7);
        } else {
            dl->AddRectFilled(p,imageEnd,pale,7);
            label(dl,{x+(cardW-measure(sans,15,"Draping garment..."))*.5f,y+imageH*.5f-8},sans,15,muted,"Draping garment...");
        }
        ImGui::PushID(int(i));
        ImGui::SetCursorScreenPos(p);
        if(ImGui::InvisibleButton(items[i].name,{cardW,imageH+caption},ImGuiButtonFlags_EnableNav)) {
            ui.wear(int(i)); ui.store=false;
        }
        bool hovered=ImGui::IsItemHovered();
        if(hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if(hovered) dl->AddRect(p,imageEnd,wine,7,0,2);
        focusOutline(dl,p,end,6);
        ImGui::PopID();
        if(ui.worn[items[i].bottom()]==int(i)) {
            dl->AddRectFilled({x+10,y+10},{x+77,y+33},wine,4);
            label(dl,{x+17,y+13},bold,12,white,"Trying on");
        }
        const float nameSize=std::min(25.f,(cardW-40)/measure(serif,1,items[i].name));
        label(dl,{x,imageEnd.y+3},serif,nameSize,ink,items[i].name);
        colorDot(dl,{x+5,imageEnd.y+40},4.8f,int(i));
        label(dl,{x+16,imageEnd.y+31},sans,13,muted,items[i].colorName);
        icon(dl,{end.x-18,imageEnd.y+6},Icon::Arrow,hovered?wine:muted,.85f);
        dl->AddLine({x,end.y},{end.x,end.y},line);
    }
    ImGui::Dummy({1,1});
    ImGui::EndChild();
}
} // namespace closet::ui_detail
