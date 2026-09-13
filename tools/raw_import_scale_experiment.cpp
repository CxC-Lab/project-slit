// Offline experiment only. Builds on raw_import_experiment.cpp's E candidate (plain
// nearest-neighbor crop of the raw reference, no offset/repair, no procedural redraw)
// but asks a different question: E's rock chunks read as too small/dense on an actual
// platform. Instead of downsampling the FULL 1254x1254 raw image to 80x80 (E0, the
// baseline this file reproduces for comparison), this crops a smaller region of the
// same raw image first, then reduces THAT to 80x80 - so the same 80x80 output canvas
// shows fewer, larger rock features.
//
// Does not touch TerrainTiles.cpp, avatar_lake.json, rock_inner_01/02.png, or any
// earlier experiment folder. Output goes to
// build/preview/avatar_lake/tiles/raw_import_scale_experiment/ only.
#include <SFML/Graphics/Image.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;
namespace {

struct Rgb { int r=0,g=0,b=0; };
sf::Color toColor(const Rgb& c) { return {static_cast<std::uint8_t>(c.r),static_cast<std::uint8_t>(c.g),static_cast<std::uint8_t>(c.b),255}; }
Rgb fromColor(sf::Color c) { return {c.r,c.g,c.b}; }
double dist2(const Rgb& a,const Rgb& b) { const double dr=a.r-b.r,dg=a.g-b.g,db=a.b-b.b; return dr*dr+dg*dg+db*db; }
double luminance(sf::Color c) { return 0.299*c.r+0.587*c.g+0.114*c.b; }

std::vector<Rgb> allPixels(const sf::Image& img)
{
    std::vector<Rgb> out; const auto size=img.getSize();
    out.reserve(size.x*size.y);
    for(unsigned y=0;y<size.y;++y) for(unsigned x=0;x<size.x;++x) out.push_back(fromColor(img.getPixel({x,y})));
    return out;
}

std::vector<Rgb> extractPalette(const std::vector<Rgb>& samples,std::size_t k)
{
    auto sorted=samples;
    std::sort(sorted.begin(),sorted.end(),[](auto a,auto b){return a.r+a.g+a.b<b.r+b.g+b.b;});
    std::vector<Rgb> centers(k);
    for(std::size_t i=0;i<k;++i) { const double pct=(i+0.5)/k; centers[i]=sorted[static_cast<std::size_t>(pct*(sorted.size()-1))]; }
    for(int iter=0;iter<10;++iter) {
        std::vector<double> sr(k),sg(k),sb(k); std::vector<int> count(k);
        for(const auto& s:samples) {
            std::size_t best=0; double bestD=dist2(s,centers[0]);
            for(std::size_t i=1;i<k;++i) { const double d=dist2(s,centers[i]); if(d<bestD){bestD=d;best=i;} }
            sr[best]+=s.r; sg[best]+=s.g; sb[best]+=s.b; ++count[best];
        }
        for(std::size_t i=0;i<k;++i) if(count[i]>0)
            centers[i]={static_cast<int>(sr[i]/count[i]),static_cast<int>(sg[i]/count[i]),static_cast<int>(sb[i]/count[i])};
    }
    std::sort(centers.begin(),centers.end(),[](auto a,auto b){return a.r+a.g+a.b<b.r+b.g+b.b;});
    return centers;
}

Rgb nearestInPalette(const Rgb& c,const std::vector<Rgb>& palette)
{
    std::size_t best=0; double bestD=dist2(c,palette[0]);
    for(std::size_t i=1;i<palette.size();++i) { const double d=dist2(c,palette[i]); if(d<bestD){bestD=d;best=i;} }
    return palette[best];
}

// Crop [x,y,w,h) out of `src`, then point-sample (nearest neighbor, no blur) down to
// dstxdst, then snap every pixel to the given palette. This is exactly what the
// earlier tool's E candidate did, generalized to an arbitrary sub-region instead of
// always the whole 1254x1254 image.
sf::Image cropAndReduce(const sf::Image& src,unsigned cx,unsigned cy,unsigned cw,unsigned ch,unsigned dst,const std::vector<Rgb>& palette)
{
    sf::Image out({dst,dst},sf::Color::Black);
    for(unsigned y=0;y<dst;++y) for(unsigned x=0;x<dst;++x) {
        const unsigned sx=cx+std::min(cw-1,static_cast<unsigned>((x+0.5)*cw/dst));
        const unsigned sy=cy+std::min(ch-1,static_cast<unsigned>((y+0.5)*ch/dst));
        out.setPixel({x,y},toColor(nearestInPalette(fromColor(src.getPixel({sx,sy})),palette)));
    }
    return out;
}

double edgeDiff(const sf::Image& a,bool horizontal)
{
    const auto size=a.getSize();
    const unsigned n=horizontal?size.y:size.x;
    double total=0;
    for(unsigned i=0;i<n;++i) {
        const auto pa=horizontal?a.getPixel({size.x-1,i}):a.getPixel({i,size.y-1});
        const auto pb=horizontal?a.getPixel({0,i}):a.getPixel({i,0});
        total+=std::abs(int(pa.r)-int(pb.r))+std::abs(int(pa.g)-int(pb.g))+std::abs(int(pa.b)-int(pb.b));
    }
    return total/(n*3.0);
}

struct BlobStats { double largestFraction; int bigBlobCount; double meanLum; double stdLum; };
// 4-connected flood fill over exact color equality (the image is already snapped to
// the palette, so equal color <=> same palette entry) - measures whether one rock
// chunk swallows the whole tile (too dominant) or everything is confetti-small
// (reads as gravel) versus a handful of readable, separate chunks.
BlobStats analyzeBlobs(const sf::Image& img)
{
    const auto size=img.getSize();
    const unsigned n=size.x*size.y;
    std::vector<bool> visited(n,false);
    std::vector<double> lums; lums.reserve(n);
    unsigned largest=0; int bigCount=0;
    for(unsigned y=0;y<size.y;++y) for(unsigned x=0;x<size.x;++x) lums.push_back(luminance(img.getPixel({x,y})));
    for(unsigned start=0;start<n;++start) {
        if(visited[start]) continue;
        const unsigned sx=start%size.x,sy=start/size.x;
        const auto color=img.getPixel({sx,sy});
        std::vector<unsigned> stack{start}; visited[start]=true; unsigned area=0;
        while(!stack.empty()) {
            const unsigned cur=stack.back(); stack.pop_back(); ++area;
            const unsigned x=cur%size.x,y=cur/size.x;
            const std::array<std::pair<int,int>,4> deltas{{{1,0},{-1,0},{0,1},{0,-1}}};
            for(auto [dx,dy]:deltas) {
                const int nx=static_cast<int>(x)+dx,ny=static_cast<int>(y)+dy;
                if(nx<0||ny<0||nx>=static_cast<int>(size.x)||ny>=static_cast<int>(size.y)) continue;
                const unsigned idx=static_cast<unsigned>(ny)*size.x+static_cast<unsigned>(nx);
                if(visited[idx]) continue;
                if(img.getPixel({static_cast<unsigned>(nx),static_cast<unsigned>(ny)})!=color) continue;
                visited[idx]=true; stack.push_back(idx);
            }
        }
        largest=std::max(largest,area);
        if(static_cast<double>(area)/n>0.05) ++bigCount;
    }
    const double mean=std::accumulate(lums.begin(),lums.end(),0.0)/n;
    double var=0; for(double l:lums) var+=(l-mean)*(l-mean); var/=n;
    return {static_cast<double>(largest)/n,bigCount,mean,std::sqrt(var)};
}

void saveOrThrow(const sf::Image& img,const fs::path& path) { if(!img.saveToFile(path)) throw std::runtime_error("save failed: "+path.string()); }

sf::Image upscale(const sf::Image& src,unsigned s)
{
    const auto size=src.getSize();
    sf::Image big({size.x*s,size.y*s},sf::Color::Black);
    for(unsigned y=0;y<size.y*s;++y) for(unsigned x=0;x<size.x*s;++x) big.setPixel({x,y},src.getPixel({x/s,y/s}));
    return big;
}

sf::Image tileRepeat(const sf::Image& tile,unsigned canvasSize)
{
    const auto t=tile.getSize();
    sf::Image out({canvasSize,canvasSize},sf::Color::Black);
    for(unsigned y=0;y<canvasSize;++y) for(unsigned x=0;x<canvasSize;++x) out.setPixel({x,y},tile.getPixel({x%t.x,y%t.y}));
    return out;
}

sf::Image withGrid(const sf::Image& src,unsigned cell)
{
    sf::Image out=src; const auto size=src.getSize();
    const sf::Color line(90,220,120,255);
    for(unsigned y=0;y<size.y;++y) for(unsigned x=0;x<size.x;++x) if(x%cell==0||y%cell==0) out.setPixel({x,y},line);
    return out;
}

} // namespace

int main() try
{
    const fs::path root=fs::current_path();
    const fs::path sourceDir=root/"assets/tiles/avatar_lake/source";
    const fs::path outDir=root/"build/preview/avatar_lake/tiles/raw_import_scale_experiment";
    fs::create_directories(outDir);
    const fs::path repeatDir=outDir/"repeat_comparison";
    fs::create_directories(repeatDir);

    sf::Image raw1;
    if(!raw1.loadFromFile(sourceDir/"rock_raw_01.png")) throw std::runtime_error("Cannot load rock_raw_01.png");
    const unsigned rawSize=raw1.getSize().x; // 1254, square
    // Same palette source as the earlier E candidate: 14-color k-means over the WHOLE
    // raw image, kept fixed across all three crop sizes so color treatment matches E
    // exactly - only the sampled spatial extent changes.
    const auto palette=extractPalette(allPixels(raw1),14);
    constexpr unsigned Dst=80;

    struct Candidate { std::string name; unsigned cropSize; unsigned cx,cy; sf::Image image; BlobStats stats; double edgeH,edgeV; };
    std::vector<Candidate> chosen;

    // E0: the full image, exactly reproducing the earlier E candidate (single point,
    // no search needed).
    {
        auto img=cropAndReduce(raw1,0,0,rawSize,rawSize,Dst,palette);
        const auto stats=analyzeBlobs(img);
        chosen.push_back({"E0_current",rawSize,0,0,img,stats,edgeDiff(img,true),edgeDiff(img,false)});
    }

    std::ostringstream search;
    search<<"# Crop position search\n\n";
    for(const auto& [tag,fraction] : std::vector<std::pair<std::string,double>>{{"E1_medium",0.75},{"E2_loose",0.50}}) {
        const unsigned cropSize=static_cast<unsigned>(std::round(rawSize*fraction));
        const unsigned range=rawSize-cropSize;
        search<<"## "<<tag<<" (crop "<<cropSize<<"x"<<cropSize<<" of "<<rawSize<<"x"<<rawSize<<", "<<static_cast<int>(fraction*100)<<"%)\n\n"
              <<"| cx | cy | edgeH | edgeV | largestFrac | bigBlobs | meanLum | stdLum | score |\n|---|---|---|---|---|---|---|---|---|\n";
        struct Scored { unsigned cx,cy; sf::Image img; BlobStats stats; double edgeH,edgeV,score; };
        std::vector<Scored> results;
        constexpr int Steps=6;
        for(int iy=0;iy<Steps;++iy) for(int ix=0;ix<Steps;++ix) {
            const unsigned cx=range==0?0:static_cast<unsigned>(range*ix/double(Steps-1));
            const unsigned cy=range==0?0:static_cast<unsigned>(range*iy/double(Steps-1));
            auto img=cropAndReduce(raw1,cx,cy,cropSize,cropSize,Dst,palette);
            const auto stats=analyzeBlobs(img);
            const double eh=edgeDiff(img,true),ev=edgeDiff(img,false);
            const double score=-(eh+ev)-std::abs(stats.largestFraction-0.22)*150.0+stats.stdLum*0.5;
            results.push_back({cx,cy,img,stats,eh,ev,score});
        }
        std::sort(results.begin(),results.end(),[](auto& a,auto& b){return a.score>b.score;});
        for(std::size_t i=0;i<results.size();++i) {
            const auto& r=results[i];
            search<<"| "<<r.cx<<" | "<<r.cy<<" | "<<r.edgeH<<" | "<<r.edgeV<<" | "<<r.stats.largestFraction<<" | "<<r.stats.bigBlobCount
                  <<" | "<<r.stats.meanLum<<" | "<<r.stats.stdLum<<" | "<<r.score<<(i==0?" | <- picked":"")<<" |\n";
        }
        const auto& best=results.front();
        saveOrThrow(best.img,outDir/(tag+"_debug_top_pick_only.png"));
        chosen.push_back({tag,cropSize,best.cx,best.cy,best.img,best.stats,best.edgeH,best.edgeV});
        search<<"\n";
    }
    std::cout<<search.str();
    std::ofstream searchFile(outDir/"crop_search.md");
    searchFile<<search.str();

    std::ostringstream report;
    report<<"# E0/E1/E2 candidates - crop coordinates and stats\n\n"
          <<"Source: assets/tiles/avatar_lake/source/rock_raw_01.png (1254x1254), palette: same 14-color\n"
          <<"k-means over the whole image for all three candidates (matches E's color treatment).\n\n";
    for(auto& c:chosen) {
        saveOrThrow(c.image,outDir/(c.name+".png"));
        saveOrThrow(upscale(c.image,4),outDir/(c.name+"_inspect.png"));
        report<<"## "<<c.name<<"\n"<<"- crop: ("<<c.cx<<","<<c.cy<<") size "<<c.cropSize<<"x"<<c.cropSize
              <<" -> reduced to 80x80\n"<<"- self-tile edge diff: h="<<c.edgeH<<" v="<<c.edgeV<<"\n"
              <<"- largest connected same-color fraction: "<<c.stats.largestFraction<<"\n"
              <<"- big blob count (>5% area): "<<c.stats.bigBlobCount<<"\n"
              <<"- luminance mean="<<c.stats.meanLum<<" sd="<<c.stats.stdLum<<"\n\n";

        constexpr unsigned CanvasSize=240;
        const auto plain=tileRepeat(c.image,CanvasSize);
        saveOrThrow(plain,repeatDir/(c.name+".png"));
        saveOrThrow(upscale(plain,2),repeatDir/(c.name+"_x2.png"));
        const auto grid=withGrid(plain,80);
        saveOrThrow(grid,repeatDir/(c.name+"_grid.png"));
        saveOrThrow(upscale(grid,2),repeatDir/(c.name+"_grid_x2.png"));
    }
    std::cout<<report.str();
    std::ofstream reportFile(outDir/"candidates.md");
    reportFile<<report.str();

    // ---- Scene mockups: identical setup to raw_import_experiment.cpp's
    // scene_mockup_1920x1080.png (same background offsets, same platform selection
    // rule, same character alignment) - only the tile filling the platform changes. ----
    sf::Image distant,middle,protoRef;
    const bool hasDistant=distant.loadFromFile(root/"assets/backgrounds/avatar_lake/distant.png");
    const bool hasMiddle=middle.loadFromFile(root/"assets/backgrounds/avatar_lake/middle.png");
    const bool hasProto=protoRef.loadFromFile(root/"assets/characters/proto_one/normalized/idle/proto_one_idle_00.png");
    int platformW=250,platformH=120;
    {
        std::ifstream regionFile(root/"assets/regions/avatar_lake.json");
        if(regionFile) {
            const auto regionJson=nlohmann::json::parse(regionFile);
            long bestArea=-1;
            for(const auto& s:regionJson.at("solids")) {
                const int w=s.at(2).get<int>(),h=s.at(3).get<int>();
                if(h>300||w>900) continue;
                if(static_cast<long>(w)*h>bestArea) { bestArea=static_cast<long>(w)*h; platformW=w; platformH=h; }
            }
        }
    }
    std::vector<sf::Image> mockups;
    if(hasDistant&&hasMiddle) {
        constexpr unsigned SW=1920,SH=1080;
        for(auto& c:chosen) {
            sf::Image scene({SW,SH},sf::Color(18,22,34,255));
            auto putPixel=[&](int x,int y,sf::Color col) { if(x>=0&&y>=0&&x<static_cast<int>(SW)&&y<static_cast<int>(SH)) scene.setPixel({static_cast<unsigned>(x),static_cast<unsigned>(y)},col); };
            auto blit=[&](const sf::Image& layer,int offsetX,int offsetY) {
                const auto s=layer.getSize();
                for(unsigned y=0;y<s.y;++y) for(unsigned x=0;x<s.x;++x) { const auto p=layer.getPixel({x,y}); if(p.a>10) putPixel(static_cast<int>(x)+offsetX,static_cast<int>(y)+offsetY,p); }
            };
            blit(distant,(static_cast<int>(SW)-static_cast<int>(distant.getSize().x))/2,SH-static_cast<int>(distant.getSize().y)-40);
            blit(middle,(static_cast<int>(SW)-static_cast<int>(middle.getSize().x))/2,SH-static_cast<int>(middle.getSize().y)+60);
            const int platformX=(static_cast<int>(SW)-platformW)/2,platformY=SH-260;
            for(int y=0;y<platformH;++y) for(int x=0;x<platformW;++x)
                putPixel(platformX+x,platformY+y,c.image.getPixel({static_cast<unsigned>(x)%80u,static_cast<unsigned>(y)%80u}));
            const sf::Color diagnosticBorder(255,0,180,255);
            for(int x=0;x<platformW;++x) { putPixel(platformX+x,platformY,diagnosticBorder); putPixel(platformX+x,platformY+platformH-1,diagnosticBorder); }
            for(int y=0;y<platformH;++y) { putPixel(platformX,platformY+y,diagnosticBorder); putPixel(platformX+platformW-1,platformY+y,diagnosticBorder); }
            if(hasProto) {
                const int protoX=platformX+platformW/2-64,protoY=platformY-123;
                const auto ps=protoRef.getSize();
                for(unsigned y=0;y<ps.y&&y<128;++y) for(unsigned x=0;x<ps.x&&x<128;++x) {
                    const auto p=protoRef.getPixel({x,y});
                    if(p.a>10) putPixel(protoX+static_cast<int>(x),protoY+static_cast<int>(y),p);
                }
            }
            saveOrThrow(scene,outDir/("scene_mockup_"+c.name+".png"));
            mockups.push_back(scene);
        }
        // Side-by-side comparison strip: each mockup downscaled 3x (640x360), placed
        // side by side with a thin separator.
        constexpr unsigned PW=640,PH=360,Gap=6;
        sf::Image strip({static_cast<unsigned>(mockups.size())*PW+(static_cast<unsigned>(mockups.size())-1)*Gap,PH},sf::Color(255,255,255,255));
        for(std::size_t i=0;i<mockups.size();++i) {
            for(unsigned y=0;y<PH;++y) for(unsigned x=0;x<PW;++x)
                strip.setPixel({static_cast<unsigned>(i)*(PW+Gap)+x,y},mockups[i].getPixel({x*3,y*3}));
        }
        saveOrThrow(strip,outDir/"scene_mockup_side_by_side.png");
        std::cout<<"Scene mockups + side-by-side strip written.\n";
    } else {
        std::cerr<<"WARNING: could not load background layers, skipped scene mockups\n";
    }
}
catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<"\n"; return 1; }
