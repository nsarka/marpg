#include <SFML/Graphics.hpp>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <vector>
// Approximate surface normals from the original tiles' three-tone material palette.
// Outputs preserve every original pixel and alpha; these are editable starting maps.
int main(int argc,char** argv) {
    if(argc!=2)return 1;
    const std::filesystem::path source=argv[1],out=source.parent_path()/"Lighting";
    std::filesystem::create_directories(out);
    for(const auto& entry:std::filesystem::directory_iterator(source)) {
        if(entry.path().extension()!=".png")continue;
        sf::Image image;if(!image.loadFromFile(entry.path()))return 2;
        auto size=image.getSize();sf::Image albedo=image,normal(size,sf::Color(128,128,255,0));
        const auto name=entry.path().stem().string();
        const bool floor=name=="floor_E" || name=="switchFloorOn_E";
        unsigned faces[3]{};
        sf::Image height(size,sf::Color::Transparent);
        std::vector<int> bottoms(size.x,0);
        for(unsigned x=0;x<size.x;++x)for(unsigned y=0;y<size.y;++y)
            if(image.getPixel({x,y}).a>128)bottoms[x]=y;
        for(unsigned y=0;y<size.y;++y)for(unsigned x=0;x<size.x;++x) {
            auto c=image.getPixel({x,y});if(!c.a)continue;
            // Orange faces: dark = left, medium = right, bright = top.
            const bool orange=c.r>c.b*1.5f && c.g>c.b*1.2f;
            int face=floor?2:orange?(c.r<205?0:c.g<160?1:2):2;
            if(!floor && orange) {
                int votes[3]{};
                for(int dy=-3;dy<=3;++dy)for(int dx=-3;dx<=3;++dx) {
                    const int xx=int(x)+dx,yy=int(y)+dy;
                    if(xx<0 || yy<0 || xx>=int(size.x) || yy>=int(size.y))continue;
                    auto n=image.getPixel({unsigned(xx),unsigned(yy)});
                    if(n.a && n.r>n.b*1.5f && n.g>n.b*1.2f)++votes[n.r<205?0:n.g<160?1:2];
                }
                face=int(std::max_element(votes,votes+3)-votes);
            }
            if(!floor && !orange) {
                // Insets inherit the adjacent structural face; material brightness
                // alone must not turn a dark window into a differently facing wall.
                int best=999;
                for(int dy=-24;dy<=24;dy+=3)for(int dx=-24;dx<=24;dx+=3) {
                    const int xx=int(x)+dx,yy=int(y)+dy,d=dx*dx+dy*dy;
                    if(xx<0 || yy<0 || xx>=int(size.x) || yy>=int(size.y) || d>=best)continue;
                    auto n=image.getPixel({unsigned(xx),unsigned(yy)});
                    if(n.a && n.r>n.b*1.5f && n.g>n.b*1.2f) {face=n.r<205?0:n.g<160?1:2;best=d;}
                }
            }
            // These stairs have 32 px treads and 20 px risers, repeating
            // every 52 projected pixels. Shadows along their seams are not
            // changes in surface orientation. Use the authored geometry.
            if(name=="stairs_E" || name=="stairs_S" || name=="stairsCornerOuter_S") {
                const float xx=name=="stairs_S"?255.f-x:float(x);
                if(name=="stairsCornerOuter_S") {
                    const float q=float(y)+.5f*std::abs(float(x)-127.5f);
                    const float phase=q-304.f-52.f*std::floor((q-304.f)/52.f);
                    face=phase<32.f?2:(x<128?0:1);
                } else {
                    const float sideEdge=368.f+.5f*xx+20.f*std::floor(xx/32.f);
                    const float q=float(y)+.5f*xx-368.f;
                    const float phase=q-52.f*std::floor(q/52.f);
                    face=xx<128.f && y>=sideEdge?(name=="stairs_E"?0:1):
                        (phase<32.f?2:(name=="stairs_E"?1:0));
                }
            }
            float elevation=0;
            if(!floor) {
                const float maximum=name=="crate_E"?80.f:name=="fence_W"?64.f:160.f;
                elevation=face==2?maximum:std::clamp(float(bottoms[x])-y,0.f,maximum);
                if(name=="stairs_E" || name=="stairs_S") {
                    const float xx=name=="stairs_S"?255.f-x:float(x);
                    const float q=float(y)+.5f*xx-368.f;
                    const float step=std::clamp(std::floor(q/52.f),0.f,3.f);
                    const float phase=q-step*52.f;
                    elevation=face==2?80.f-step*20.f:
                        face==(name=="stairs_E"?1:0)?80.f-step*20.f-std::max(0.f,phase-32.f):float(bottoms[x])-y;
                } else if(name=="stairsCornerOuter_S") {
                    const float q=float(y)+.5f*std::abs(float(x)-127.5f)-304.f;
                    const float step=std::clamp(std::floor(q/52.f),0.f,3.f);
                    elevation=80.f-step*20.f-(face==2?0.f:std::max(0.f,q-step*52.f-32.f));
                }
            }
            float fenceBottom=0;
            if(name=="fence_W") {
                // Post caps are 40 pixels above their ground diamonds. Rails
                // follow the sloping fence baseline, not the lowest opaque
                // pixel in each column (which is the rail itself).
                const bool post=x<136 || x>=240;
                if(post) {
                    const float center=x<136?128.f:248.f;
                    const float cap=x<136?472.f:412.f;
                    face=y<cap-.5f*std::abs(float(x)-center)?2:(x<center?0:1);
                    elevation=face==2?40.f:std::clamp(float(bottoms[x])-y,0.f,40.f);
                } else {
                    const float q=y+.5f*x;
                    const bool upper=q<553.f;
                    const float top=upper?32.f:22.f;
                    fenceBottom=upper?24.f:14.f;
                    face=q<(upper?547.f:557.f)?2:1;
                    elevation=face==2?top:std::clamp(576.f-q,fenceBottom,top);
                }
            }
            elevation=std::clamp(elevation,0.f,510.f);
            // Red: surface elevation in two-pixel units. Green: bottom of
            // solid volume; the doorway lintel leaves an opening underneath.
            const float bottom=name=="fence_W"?fenceBottom:name=="doorway_E" && x>=64 && x<=128 && y+.5f*x<364.f?std::min(elevation,112.f):0.f;
            height.setPixel({x,y},sf::Color(std::uint8_t(std::round(elevation/2.f)),std::uint8_t(bottom/2.f),0,c.a));
            ++faces[face];
            // View-space normals: X right, Y down-screen, Z toward the viewer.
            sf::Vector3f n=face==0?sf::Vector3f{-.8165f,.4082f,.4082f}:face==1?sf::Vector3f{.8165f,.4082f,.4082f}:sf::Vector3f{0,-.8165f,.57735f};
            normal.setPixel({x,y},sf::Color(std::uint8_t((n.x*.5f+.5f)*255),std::uint8_t((n.y*.5f+.5f)*255),std::uint8_t((n.z*.5f+.5f)*255),c.a));
            // Remove broad directional exposure without repainting the material.
            // One scalar preserves channel ratios, including mortar, lettering,
            // bevels and the subtle hue differences in the original artwork.
            float factor=orange?(face==0?240.f/170.f:face==1?240.f/238.f:240.f/255.f):
                (face==0?1.20f:face==1?1.04f:1.f);
            const float peak=std::max({c.r,c.g,c.b});
            if(peak>0)factor=std::min(factor,255.f/peak);
            albedo.setPixel({x,y},sf::Color(std::uint8_t(std::round(c.r*factor)),
                std::uint8_t(std::round(c.g*factor)),std::uint8_t(std::round(c.b*factor)),c.a));
        }
        if(!height.saveToFile(out/(name+".height.png")))return 3;
        if(!albedo.saveToFile(out/(name+".albedo.png")) || !normal.saveToFile(out/(name+".normal.png")))return 3;
        std::cout<<name<<": "<<faces[0]<<" left, "<<faces[1]<<" right, "<<faces[2]<<" top pixels\n";
    }
}
