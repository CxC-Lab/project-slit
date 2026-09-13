// One-off asset-prep tool: builds Avatar Lake's "inner" rock tile(s) at the game's
// actual tile size from the ChatGPT-generated raw references, instead of resizing them.
// See build/preview/avatar_lake/tiles/normalization_report.md for the rationale.
#include <SFML/Graphics/Image.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;
namespace {

struct Rgb { int r=0,g=0,b=0; };
sf::Color toColor(const Rgb& c) { return {static_cast<std::uint8_t>(c.r),static_cast<std::uint8_t>(c.g),static_cast<std::uint8_t>(c.b),255}; }
double luminance(const Rgb& c) { return 0.299*c.r+0.587*c.g+0.114*c.b; }
double dist2(const Rgb& a,const Rgb& b) { const double dr=a.r-b.r,dg=a.g-b.g,db=a.b-b.b; return dr*dr+dg*dg+db*db; }

std::vector<Rgb> samplePixels(const sf::Image& image,unsigned stride)
{
    std::vector<Rgb> out; const auto size=image.getSize();
    for(unsigned y=0;y<size.y;y+=stride) for(unsigned x=0;x<size.x;x+=stride) {
        const auto p=image.getPixel({x,y}); out.push_back({p.r,p.g,p.b});
    }
    return out;
}

// Deterministic k-means (fixed percentile seeding, fixed iteration count): same
// inputs always produce the same six-color palette, so this step is reproducible.
std::array<Rgb,6> extractPalette(const std::vector<Rgb>& samples)
{
    auto sorted=samples;
    std::sort(sorted.begin(),sorted.end(),[](auto a,auto b){return luminance(a)<luminance(b);});
    std::array<Rgb,6> centers{};
    constexpr std::array<double,6> pct{0.04,0.20,0.38,0.56,0.75,0.94};
    for(std::size_t i=0;i<6;++i) centers[i]=sorted[static_cast<std::size_t>(pct[i]*(sorted.size()-1))];
    for(int iter=0;iter<8;++iter) {
        std::array<double,6> sr{},sg{},sb{}; std::array<int,6> count{};
        for(const auto& s:samples) {
            std::size_t best=0; double bestD=dist2(s,centers[0]);
            for(std::size_t i=1;i<6;++i) { const double d=dist2(s,centers[i]); if(d<bestD){bestD=d;best=i;} }
            sr[best]+=s.r; sg[best]+=s.g; sb[best]+=s.b; ++count[best];
        }
        for(std::size_t i=0;i<6;++i) if(count[i]>0)
            centers[i]={static_cast<int>(sr[i]/count[i]),static_cast<int>(sg[i]/count[i]),static_cast<int>(sb[i]/count[i])};
    }
    std::sort(centers.begin(),centers.end(),[](auto a,auto b){return luminance(a)<luminance(b);});
    return centers;
}

// The raw references sample brighter and flatter than the Avatar Lake background
// layers (measured: distant.png opaque pixels mean=131 sd=32, middle.png mean=79
// sd=35, vs the extracted palette's untouched mean=~131 sd=~24). Foreground terrain
// is meant to read a bit darker and higher-contrast than the distant layer, so pull
// the palette's per-channel spread out around its own mean and shift it down.
std::array<Rgb,6> tuneForForeground(std::array<Rgb,6> p,double gain,double offset)
{
    double meanR=0,meanG=0,meanB=0;
    for(const auto& c:p) { meanR+=c.r; meanG+=c.g; meanB+=c.b; }
    meanR/=6; meanG/=6; meanB/=6;
    auto adjust=[&](int v,double mean) { return static_cast<int>(std::clamp(mean+(v-mean)*gain+offset,0.0,255.0)); };
    for(auto& c:p) { c={adjust(c.r,meanR),adjust(c.g,meanG),adjust(c.b,meanB)}; }
    return p;
}

constexpr unsigned T=20;
// Column layout for a 20px-wide inner tile: valleys sit at x=0, 7, 14 and 19.
// x=0 and x=19 are BOTH valley pixels using the identical palette entry, so the
// self-repeat seam (column 19 of one copy against column 0 of the next) is not just
// visually close but bit-identical by construction, for every variant.
struct Segment { int kind; int local; int width; }; // kind: 0=valley,1=colA,2=colB,3=colC
Segment segmentAt(unsigned x)
{
    if(x==0||x==7||x==14||x==19) return {0,0,1};
    if(x>=1&&x<=6) return {1,static_cast<int>(x-1),6};
    if(x>=8&&x<=13) return {2,static_cast<int>(x-8),6};
    return {3,static_cast<int>(x-15),4}; // 15..18
}

// Cheap deterministic per-pixel hash for grain dithering (same shape as the
// engine's own tileHash in TerrainTiles.cpp, kept local since this tool doesn't
// link that translation unit): identical inputs always give identical output.
std::uint32_t grainHash(unsigned x,unsigned y,unsigned variant)
{
    std::uint32_t h=x*374761393u+y*668265263u+variant*2654435761u;
    h=(h^(h>>13))*1274126177u;
    return h^(h>>16);
}

sf::Image buildTile(const std::array<Rgb,6>& p,unsigned variant,const std::vector<std::pair<unsigned,unsigned>>& speckles)
{
    // Each column gets its own band phase so the horizontal sediment lines stagger
    // between columns instead of ruling straight across the whole tile - three
    // separate rock spires read as independent, not one repeating ruler pattern.
    const std::array<unsigned,4> phase{0,0,2,4}; // indexed by seg.kind (0 unused)
    sf::Image image({T,T},sf::Color::Black);
    for(unsigned y=0;y<T;++y) for(unsigned x=0;x<T;++x) {
        const auto seg=segmentAt(x);
        Rgb color;
        if(seg.kind==0) color=p[0]; // valley: darkest, gray-purple shadow. No grain: keeps the seam columns pixel-identical across variants.
        else {
            const bool edgeLeft=seg.local==0;
            const bool edgeRight=seg.local>=seg.width-2;
            const bool band=(y+phase[seg.kind])%5==0;
            const Rgb base=(seg.kind==2)?p[3]:p[2];
            const Rgb banded=(seg.kind==2)?p[2]:p[1];
            if(edgeLeft) color=p[1];             // soft transition out of the valley
            else if(edgeRight) color=band?base:p[4]; // sunlit face, broken into segments by the band phase instead of one solid bar
            else {
                color=band?banded:base;
                // Grain: deterministic per-pixel dither, ~1 in 6 body pixels nudged one
                // palette step darker, so flat fills read as weathered rock, not a flat fill.
                if(grainHash(x,y,variant)%6==0) color=(seg.kind==2)?p[1]:p[0];
            }
        }
        image.setPixel({x,y},toColor(color));
    }
    // Speckle chips never land on a valley column, so the seam columns (and thus
    // cross-variant tiling) stay identical between rock_inner_01 and rock_inner_02.
    for(auto [sx,sy]:speckles) if(segmentAt(sx).kind!=0) image.setPixel({sx,sy},toColor(p[5]));
    return image;
}

// Average absolute per-channel difference between the touching edge of `a` and the
// opposite edge of `b` when `b` is placed immediately after `a` (right of, or below).
double edgeDiff(const sf::Image& a,const sf::Image& b,bool horizontal)
{
    double total=0;
    for(unsigned i=0;i<T;++i) {
        const auto pa=horizontal?a.getPixel({T-1,i}):a.getPixel({i,T-1});
        const auto pb=horizontal?b.getPixel({0,i}):b.getPixel({i,0});
        total+=std::abs(static_cast<int>(pa.r)-static_cast<int>(pb.r))
              +std::abs(static_cast<int>(pa.g)-static_cast<int>(pb.g))
              +std::abs(static_cast<int>(pa.b)-static_cast<int>(pb.b));
    }
    return total/(T*3.0);
}

void tileInto(sf::Image& canvas,const sf::Image& tile,unsigned cx,unsigned cy)
{
    for(unsigned y=0;y<T;++y) for(unsigned x=0;x<T;++x) canvas.setPixel({cx*T+x,cy*T+y},tile.getPixel({x,y}));
}

// ---- Exploratory only, below this line -----------------------------------
// Not a shipped asset: simulates the "40x40 seamless texture split into a 2x2
// phase-selected meta-tile" idea (each phase quadrant would replace the current
// per-cell tileHash()%variants pick for the inner role in TerrainTiles.cpp, which
// this tool does not touch). Produced purely to compare against the shipped
// 20x20 rock_inner_01/02 before deciding whether that engine change is worth it.
constexpr unsigned MetaW=40;
struct MetaSegment { int kind; int local; int width; }; // kind: 0=valley, 1..4=columns
MetaSegment metaSegmentAt(unsigned x)
{
    if(x==0||x==11||x==21||x==30||x==39) return {0,0,1};
    if(x>=1&&x<=10) return {1,static_cast<int>(x-1),10};
    if(x>=12&&x<=20) return {2,static_cast<int>(x-12),9};
    if(x>=22&&x<=29) return {3,static_cast<int>(x-22),8};
    return {4,static_cast<int>(x-31),8}; // 31..38
}

sf::Image buildMetaTile(const std::array<Rgb,6>& p,unsigned variant,const std::vector<std::pair<unsigned,unsigned>>& speckles)
{
    const std::array<unsigned,5> phase{0,0,1,3,4};       // by kind, kind 0 unused
    const std::array<unsigned,5> highlightRun{0,2,2,3,2}; // kind 3 gets a taller sunlit face - one bigger spire, still not a repeating landmark since it's part of the tile's own periodic content
    sf::Image image({MetaW,MetaW},sf::Color::Black);
    for(unsigned y=0;y<MetaW;++y) for(unsigned x=0;x<MetaW;++x) {
        const auto seg=metaSegmentAt(x);
        Rgb color;
        if(seg.kind==0) color=p[0];
        else {
            const bool edgeLeft=seg.local==0;
            const bool edgeRight=static_cast<unsigned>(seg.local)>=seg.width-static_cast<int>(highlightRun[seg.kind]);
            const bool band=(y+phase[seg.kind])%5==0;
            const bool evenKind=seg.kind%2==0;
            const Rgb base=evenKind?p[3]:p[2];
            const Rgb banded=evenKind?p[2]:p[1];
            if(edgeLeft) color=p[1];
            else if(edgeRight) color=band?base:p[4];
            else {
                color=band?banded:base;
                if(grainHash(x,y,variant)%6==0) color=evenKind?p[1]:p[0];
            }
        }
        image.setPixel({x,y},toColor(color));
    }
    for(auto [sx,sy]:speckles) if(metaSegmentAt(sx).kind!=0) image.setPixel({sx,sy},toColor(p[5]));
    return image;
}

double metaEdgeDiff(const sf::Image& a,const sf::Image& b,bool horizontal)
{
    double total=0;
    for(unsigned i=0;i<MetaW;++i) {
        const auto pa=horizontal?a.getPixel({MetaW-1,i}):a.getPixel({i,MetaW-1});
        const auto pb=horizontal?b.getPixel({0,i}):b.getPixel({i,0});
        total+=std::abs(static_cast<int>(pa.r)-static_cast<int>(pb.r))
              +std::abs(static_cast<int>(pa.g)-static_cast<int>(pb.g))
              +std::abs(static_cast<int>(pa.b)-static_cast<int>(pb.b));
    }
    return total/(MetaW*3.0);
}

} // namespace

int main() try
{
    const fs::path root=fs::current_path();
    const fs::path sourceDir=root/"assets/tiles/avatar_lake/source";
    const fs::path normalizedDir=root/"assets/tiles/avatar_lake/normalized";
    const fs::path previewDir=root/"build/preview/avatar_lake/tiles";
    fs::create_directories(normalizedDir);
    fs::create_directories(previewDir);

    std::ifstream tilesetFile(root/"assets/tilesets/avatar_lake.json");
    if(!tilesetFile) throw std::runtime_error("Cannot open assets/tilesets/avatar_lake.json");
    const auto tilesetJson=nlohmann::json::parse(tilesetFile);
    const int declaredSize=tilesetJson.at("tileSize").get<int>();
    if(declaredSize!=static_cast<int>(T))
        std::cerr<<"WARNING: avatar_lake.json tileSize="<<declaredSize<<" but this tool assumes "<<T<<" - stop and re-check before trusting the output.\n";

    sf::Image raw1,raw2;
    if(!raw1.loadFromFile(sourceDir/"rock_raw_01.png")) throw std::runtime_error("Cannot load rock_raw_01.png");
    if(!raw2.loadFromFile(sourceDir/"rock_raw_02.png")) throw std::runtime_error("Cannot load rock_raw_02.png");

    auto samples=samplePixels(raw1,7);
    const auto s2=samplePixels(raw2,7);
    samples.insert(samples.end(),s2.begin(),s2.end());
    const auto palette=tuneForForeground(extractPalette(samples),1.35,-12);

    const std::vector<std::pair<unsigned,unsigned>> speck1{{3,3},{10,9},{16,14},{4,16}};
    const std::vector<std::pair<unsigned,unsigned>> speck2{{2,8},{11,3},{17,17},{9,13}};
    const auto tile1=buildTile(palette,0,speck1);
    const auto tile2=buildTile(palette,1,speck2);
    if(!tile1.saveToFile(normalizedDir/"rock_inner_01.png")) throw std::runtime_error("save rock_inner_01 failed");
    if(!tile2.saveToFile(normalizedDir/"rock_inner_02.png")) throw std::runtime_error("save rock_inner_02 failed");

    // Nearest-neighbor upscale for human inspection only (not a game asset).
    // `dest` is resolved relative to previewDir unless it is already absolute.
    auto upscale=[&](const sf::Image& src,unsigned s,const fs::path& dest) {
        const auto size=src.getSize();
        sf::Image big({size.x*s,size.y*s},sf::Color::Black);
        for(unsigned y=0;y<size.y*s;++y) for(unsigned x=0;x<size.x*s;++x)
            big.setPixel({x,y},src.getPixel({x/s,y/s}));
        if(!big.saveToFile(previewDir/dest)) throw std::runtime_error("upscale save failed: "+dest.string());
    };

    constexpr unsigned N=8;
    auto repeat=[&](const sf::Image& tile,const std::string& name) {
        sf::Image canvas({N*T,N*T},sf::Color::Black);
        for(unsigned cy=0;cy<N;++cy) for(unsigned cx=0;cx<N;++cx) tileInto(canvas,tile,cx,cy);
        if(!canvas.saveToFile(previewDir/name)) throw std::runtime_error("preview save failed: "+name);
        return canvas;
    };
    const auto repeat1=repeat(tile1,"rock_inner_01_repeat_8x8.png");
    const auto repeat2=repeat(tile2,"rock_inner_02_repeat_8x8.png");
    upscale(repeat1,4,"rock_inner_01_repeat_8x8_x4_inspect.png");
    upscale(repeat2,4,"rock_inner_02_repeat_8x8_x4_inspect.png");
    upscale(tile1,12,"rock_inner_01_x12_inspect.png");
    upscale(tile2,12,"rock_inner_02_x12_inspect.png");

    sf::Image checker({N*T,N*T},sf::Color::Black);
    for(unsigned cy=0;cy<N;++cy) for(unsigned cx=0;cx<N;++cx) tileInto(checker,((cx+cy)%2==0)?tile1:tile2,cx,cy);
    if(!checker.saveToFile(previewDir/"rock_inner_mixed_checker.png")) throw std::runtime_error("checker preview save failed");
    upscale(checker,4,"rock_inner_mixed_checker_x4_inspect.png");

    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> coin(0,1);
    sf::Image random({N*T,N*T},sf::Color::Black);
    for(unsigned cy=0;cy<N;++cy) for(unsigned cx=0;cx<N;++cx) tileInto(random,coin(rng)==0?tile1:tile2,cx,cy);
    if(!random.saveToFile(previewDir/"rock_inner_mixed_random.png")) throw std::runtime_error("random preview save failed");
    upscale(random,4,"rock_inner_mixed_random_x4_inspect.png");

    std::ostringstream report;
    report<<"palette (dark to light):\n";
    for(std::size_t i=0;i<6;++i) report<<"  p"<<i<<" = ("<<palette[i].r<<","<<palette[i].g<<","<<palette[i].b<<")\n";
    report<<"\nedge diff, avg abs channel delta 0-255 (0 = identical pixels at the seam):\n";
    auto line=[&](const char* label,const sf::Image& a,const sf::Image& b) {
        report<<"  "<<label<<": horizontal="<<edgeDiff(a,b,true)<<" vertical="<<edgeDiff(a,b,false)<<"\n";
    };
    line("01 -> 01",tile1,tile1);
    line("02 -> 02",tile2,tile2);
    line("01 -> 02",tile1,tile2);
    line("02 -> 01",tile2,tile1);

    std::ofstream metricsFile(previewDir/"seam_metrics.txt");
    metricsFile<<report.str();
    std::cout<<report.str();
    std::cout<<"Normalized tiles and previews written.\n";

    // ---- Exploratory 40x40 meta-tile comparison (not a shipped asset) --------
    const fs::path metaDir=previewDir/"meta_experiment";
    fs::create_directories(metaDir);
    const std::vector<std::pair<unsigned,unsigned>> metaSpeck{{4,4},{18,8},{25,15},{35,20},{7,28},{16,33},{28,36},{3,17}};
    const auto meta=buildMetaTile(palette,2,metaSpeck);
    if(!meta.saveToFile(metaDir/"meta_inner_40.png")) throw std::runtime_error("meta tile save failed");
    upscale(meta,12,fs::path("meta_experiment")/"meta_inner_40_x12_inspect.png");

    // Same 160x160 overall canvas as the shipped rock_inner_*_repeat_8x8.png, so the
    // two are a fair side-by-side: 4x4 copies of a 40px tile covers the same area as
    // 8x8 copies of a 20px tile.
    constexpr unsigned MetaN=4;
    sf::Image metaRepeat({MetaN*MetaW,MetaN*MetaW},sf::Color::Black);
    for(unsigned cy=0;cy<MetaN;++cy) for(unsigned cx=0;cx<MetaN;++cx)
        for(unsigned y=0;y<MetaW;++y) for(unsigned x=0;x<MetaW;++x)
            metaRepeat.setPixel({cx*MetaW+x,cy*MetaW+y},meta.getPixel({x,y}));
    if(!metaRepeat.saveToFile(metaDir/"meta_inner_40_repeat_4x4_equivalent.png")) throw std::runtime_error("meta repeat save failed");
    upscale(metaRepeat,4,fs::path("meta_experiment")/"meta_inner_40_repeat_4x4_equivalent_x4_inspect.png");

    std::ostringstream metaReport;
    metaReport<<"40x40 meta-tile self edge diff (avg abs channel delta 0-255):\n"
              <<"  horizontal="<<metaEdgeDiff(meta,meta,true)<<" vertical="<<metaEdgeDiff(meta,meta,false)<<"\n";
    std::ofstream metaMetricsFile(metaDir/"meta_seam_metrics.txt");
    metaMetricsFile<<metaReport.str();
    std::cout<<metaReport.str();
    std::cout<<"Meta-tile comparison experiment written to "<<metaDir.string()<<"\n";
}
catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<"\n"; return 1; }
