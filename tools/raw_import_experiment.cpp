// Offline experiment only. Does NOT touch TerrainTiles.cpp, avatar_lake.json, or
// assets/tiles/avatar_lake/normalized/rock_inner_01.png / rock_inner_02.png (those
// stay as the earlier, now-superseded normalization attempt, kept as a record).
//
// Question this answers: instead of extracting a palette and re-composing a new
// procedural pattern (what normalize_avatar_lake_tiles.cpp did, and what made the
// result look like a barcode), can the raw reference's OWN large-scale rock shapes
// be brought back as an actual game-sized texture?
//
// Output goes to build/preview/avatar_lake/tiles/raw_import_experiment/ only.
#include <SFML/Graphics/Image.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <tuple>
#include <vector>

namespace fs = std::filesystem;
namespace {

struct Rgb { int r=0,g=0,b=0; };
sf::Color toColor(const Rgb& c) { return {static_cast<std::uint8_t>(c.r),static_cast<std::uint8_t>(c.g),static_cast<std::uint8_t>(c.b),255}; }
Rgb fromColor(sf::Color c) { return {c.r,c.g,c.b}; }
double dist2(const Rgb& a,const Rgb& b) { const double dr=a.r-b.r,dg=a.g-b.g,db=a.b-b.b; return dr*dr+dg*dg+db*db; }
int chanDiff(sf::Color a,sf::Color b) { return std::abs(int(a.r)-int(b.r))+std::abs(int(a.g)-int(b.g))+std::abs(int(a.b)-int(b.b)); }

// ---- Step 1: how big is one "logical pixel" in the 1254x1254 raw reference? ----
// Walk scanlines/columns, mark a boundary wherever the color jumps by more than a
// noise threshold, and histogram the run lengths between boundaries. AI-generated
// pixel art isn't perfectly gridded, so this gives a distribution, not one exact
// number - we read off the dominant small-run mode as the effective block size.
std::map<int,int> runLengthHistogram(const sf::Image& img,int threshold)
{
    std::map<int,int> hist;
    const auto size=img.getSize();
    for(unsigned y=0;y<size.y;y+=3) {
        int runStart=0;
        for(unsigned x=1;x<size.x;++x) {
            if(chanDiff(img.getPixel({x,y}),img.getPixel({x-1,y}))>threshold) {
                const int run=static_cast<int>(x)-runStart;
                if(run>=2&&run<=60) hist[run]++;
                runStart=static_cast<int>(x);
            }
        }
    }
    for(unsigned x=0;x<size.x;x+=3) {
        int runStart=0;
        for(unsigned y=1;y<size.y;++y) {
            if(chanDiff(img.getPixel({x,y}),img.getPixel({x,y-1}))>threshold) {
                const int run=static_cast<int>(y)-runStart;
                if(run>=2&&run<=60) hist[run]++;
                runStart=static_cast<int>(y);
            }
        }
    }
    return hist;
}

// ---- Step 2: shape-preserving downsample. Point-sample (nearest neighbor) at the
// mapped source coordinate - no averaging/blur, so it doesn't invent new colors or
// soften the source's hard block edges. This is the opposite of the earlier tool,
// which threw the source's spatial layout away entirely and painted a new one. ----
sf::Image downsampleNearest(const sf::Image& src,unsigned dstW,unsigned dstH)
{
    const auto s=src.getSize();
    sf::Image out({dstW,dstH},sf::Color::Black);
    for(unsigned y=0;y<dstH;++y) for(unsigned x=0;x<dstW;++x) {
        const unsigned sx=std::min(s.x-1,static_cast<unsigned>((x+0.5)*s.x/dstW));
        const unsigned sy=std::min(s.y-1,static_cast<unsigned>((y+0.5)*s.y/dstH));
        out.setPixel({x,y},src.getPixel({sx,sy}));
    }
    return out;
}

std::vector<Rgb> allPixels(const sf::Image& img)
{
    std::vector<Rgb> out; const auto size=img.getSize();
    out.reserve(size.x*size.y);
    for(unsigned y=0;y<size.y;++y) for(unsigned x=0;x<size.x;++x) out.push_back(fromColor(img.getPixel({x,y})));
    return out;
}

// Deterministic k-means, k configurable (12-16 per the brief, vs the 6 the
// earlier tool used) - more room to keep distinct rock/shadow/highlight bands
// instead of collapsing them into a handful of procedural roles.
std::vector<Rgb> extractPalette(const std::vector<Rgb>& samples,std::size_t k)
{
    auto sorted=samples;
    std::sort(sorted.begin(),sorted.end(),[](auto a,auto b){return a.r+a.g+a.b<b.r+b.g+b.b;});
    std::vector<Rgb> centers(k);
    for(std::size_t i=0;i<k;++i) {
        const double pct=(i+0.5)/k;
        centers[i]=sorted[static_cast<std::size_t>(pct*(sorted.size()-1))];
    }
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

sf::Image quantize(const sf::Image& src,const std::vector<Rgb>& palette)
{
    const auto size=src.getSize();
    sf::Image out(size,sf::Color::Black);
    for(unsigned y=0;y<size.y;++y) for(unsigned x=0;x<size.x;++x)
        out.setPixel({x,y},toColor(nearestInPalette(fromColor(src.getPixel({x,y})),palette)));
    return out;
}

void saveOrThrow(const sf::Image& img,const fs::path& path)
{
    if(!img.saveToFile(path)) throw std::runtime_error("save failed: "+path.string());
}

sf::Image upscale(const sf::Image& src,unsigned s)
{
    const auto size=src.getSize();
    sf::Image big({size.x*s,size.y*s},sf::Color::Black);
    for(unsigned y=0;y<size.y*s;++y) for(unsigned x=0;x<size.x*s;++x)
        big.setPixel({x,y},src.getPixel({x/s,y/s}));
    return big;
}

// ---- Step 3: toroidal offset. The macro downsample above is a straight crop of the
// raw reference, so its own left/right and top/bottom edges don't match - shifting by
// half the canvas (with wraparound) moves BOTH original edges to a cross through the
// center, where they can be spot-repaired without touching the other ~90% of the image. ----
sf::Image toroidalOffset(const sf::Image& src)
{
    const auto size=src.getSize();
    sf::Image out(size,sf::Color::Black);
    for(unsigned y=0;y<size.y;++y) for(unsigned x=0;x<size.x;++x)
        out.setPixel({x,y},src.getPixel({(x+size.x/2)%size.x,(y+size.y/2)%size.y}));
    return out;
}

// Minimal repair: only pixels within `band` of the new center cross are touched, and
// even those are re-quantized to the SAME palette (never a blended/new color), just
// chosen from a local neighborhood average instead of the original single pixel. This
// softens the offset cut without redrawing the image - no new column pattern, no
// reshaping of anything outside the seam band.
sf::Image repairCenterSeam(const sf::Image& src,unsigned band,const std::vector<Rgb>& palette)
{
    const auto size=src.getSize();
    sf::Image out=src;
    const int cx=static_cast<int>(size.x/2),cy=static_cast<int>(size.y/2);
    for(int y=0;y<static_cast<int>(size.y);++y) for(int x=0;x<static_cast<int>(size.x);++x) {
        const bool nearVerticalSeam=std::abs(x-cx)<=static_cast<int>(band)||std::abs(x-cx-static_cast<int>(size.x))<=static_cast<int>(band)||std::abs(x-cx+static_cast<int>(size.x))<=static_cast<int>(band);
        const bool nearHorizontalSeam=std::abs(y-cy)<=static_cast<int>(band)||std::abs(y-cy-static_cast<int>(size.y))<=static_cast<int>(band)||std::abs(y-cy+static_cast<int>(size.y))<=static_cast<int>(band);
        if(!nearVerticalSeam&&!nearHorizontalSeam) continue;
        long sr=0,sg=0,sb=0,n=0;
        for(int dy=-2;dy<=2;++dy) for(int dx=-2;dx<=2;++dx) {
            const int sx=(x+dx+static_cast<int>(size.x))%static_cast<int>(size.x);
            const int sy=(y+dy+static_cast<int>(size.y))%static_cast<int>(size.y);
            const auto p=src.getPixel({static_cast<unsigned>(sx),static_cast<unsigned>(sy)});
            sr+=p.r; sg+=p.g; sb+=p.b; ++n;
        }
        out.setPixel({static_cast<unsigned>(x),static_cast<unsigned>(y)},
                      toColor(nearestInPalette({static_cast<int>(sr/n),static_cast<int>(sg/n),static_cast<int>(sb/n)},palette)));
    }
    return out;
}

double edgeDiff(const sf::Image& a,const sf::Image& b,bool horizontal)
{
    const auto size=a.getSize();
    const unsigned n=horizontal?size.y:size.x;
    double total=0;
    for(unsigned i=0;i<n;++i) {
        const auto pa=horizontal?a.getPixel({size.x-1,i}):a.getPixel({i,size.y-1});
        const auto pb=horizontal?b.getPixel({0,i}):b.getPixel({i,0});
        total+=chanDiff(pa,pb);
    }
    return total/(n*3.0);
}

sf::Image withGrid(const sf::Image& src,unsigned cell)
{
    sf::Image out=src; const auto size=src.getSize();
    const sf::Color line(90,220,120,255);
    for(unsigned y=0;y<size.y;++y) for(unsigned x=0;x<size.x;++x)
        if(x%cell==0||y%cell==0) out.setPixel({x,y},line);
    return out;
}

sf::Image tileRepeat(const sf::Image& tile,unsigned canvasSize)
{
    const auto t=tile.getSize();
    sf::Image out({canvasSize,canvasSize},sf::Color::Black);
    for(unsigned y=0;y<canvasSize;++y) for(unsigned x=0;x<canvasSize;++x)
        out.setPixel({x,y},tile.getPixel({x%t.x,y%t.y}));
    return out;
}

// Step 4 (candidate D): collapse the palette's darker half to one flat "quiet base"
// tone and leave the lighter half (rock body/highlight) untouched. This is the offline
// stand-in for "quiet 20x20 base tile + macro rock decoration layered on top" without
// inventing a real two-layer alpha compositor: shadow/valley area reads as calm fill,
// the actual rock shapes are the only thing left with detail.
sf::Image quietBaseComposite(const sf::Image& macro,const std::vector<Rgb>& palette)
{
    auto sortedPalette=palette;
    std::sort(sortedPalette.begin(),sortedPalette.end(),[](auto a,auto b){return a.r+a.g+a.b<b.r+b.g+b.b;});
    const Rgb quiet=sortedPalette[1];
    const std::size_t half=sortedPalette.size()/2;
    const auto size=macro.getSize();
    sf::Image out(size,sf::Color::Black);
    for(unsigned y=0;y<size.y;++y) for(unsigned x=0;x<size.x;++x) {
        const auto c=fromColor(macro.getPixel({x,y}));
        std::size_t idx=0; double bestD=dist2(c,sortedPalette[0]);
        for(std::size_t i=1;i<sortedPalette.size();++i) { const double d=dist2(c,sortedPalette[i]); if(d<bestD){bestD=d;idx=i;} }
        out.setPixel({x,y},toColor(idx<half?quiet:c));
    }
    return out;
}

} // namespace

int main() try
{
    const fs::path root=fs::current_path();
    const fs::path sourceDir=root/"assets/tiles/avatar_lake/source";
    const fs::path outDir=root/"build/preview/avatar_lake/tiles/raw_import_experiment";
    fs::create_directories(outDir);

    sf::Image raw1,raw2;
    if(!raw1.loadFromFile(sourceDir/"rock_raw_01.png")) throw std::runtime_error("Cannot load rock_raw_01.png");
    if(!raw2.loadFromFile(sourceDir/"rock_raw_02.png")) throw std::runtime_error("Cannot load rock_raw_02.png");

    std::ostringstream analysis;
    analysis<<"# Raw source block-size analysis\n\n";
    for(const auto& [name,img] : std::vector<std::pair<std::string,const sf::Image*>>{{"rock_raw_01",&raw1},{"rock_raw_02",&raw2}}) {
        analysis<<"## "<<name<<" (source size "<<img->getSize().x<<"x"<<img->getSize().y<<")\n\n";
        for(const auto& [label,threshold] : std::vector<std::pair<std::string,int>>{{"fine grain (threshold 24)",24},{"major structure (threshold 90)",90}}) {
            const auto hist=runLengthHistogram(*img,threshold);
            int mode=0,modeCount=0; long total=0,weighted=0;
            for(auto [len,cnt]:hist) { total+=cnt; weighted+=len*cnt; if(cnt>modeCount){modeCount=cnt;mode=len;} }
            const double mean=total>0?static_cast<double>(weighted)/total:0;
            analysis<<"### "<<label<<"\n"<<"- run-length samples: "<<total<<"\n"<<"- mode run length (px): "<<mode<<" (count="<<modeCount<<")\n"
                    <<"- mean run length: "<<mean<<"\n"<<"- implied feature count across width: ~"<<static_cast<int>(img->getSize().x/std::max(1.0,mean))<<"\n"
                    <<"- histogram (run length: count), top 10:\n";
            std::vector<std::pair<int,int>> sorted(hist.begin(),hist.end());
            std::sort(sorted.begin(),sorted.end(),[](auto a,auto b){return a.second>b.second;});
            for(std::size_t i=0;i<sorted.size()&&i<10;++i) analysis<<"    "<<sorted[i].first<<": "<<sorted[i].second<<"\n";
            analysis<<"\n";
        }
    }
    std::cout<<analysis.str();
    std::ofstream analysisFile(outDir/"block_size_analysis.md");
    analysisFile<<analysis.str();

    // Macro candidates at three sizes, both raws, shape-preserving (no re-composition).
    const std::vector<unsigned> candidateSizes{40,80,120};
    const auto palette1=extractPalette(allPixels(raw1),14);
    const auto palette2=extractPalette(allPixels(raw2),14);
    for(unsigned size:candidateSizes) {
        auto d1=quantize(downsampleNearest(raw1,size,size),palette1);
        auto d2=quantize(downsampleNearest(raw2,size,size),palette2);
        saveOrThrow(d1,outDir/("macro_01_"+std::to_string(size)+".png"));
        saveOrThrow(d2,outDir/("macro_02_"+std::to_string(size)+".png"));
        saveOrThrow(upscale(d1,320/size),outDir/("macro_01_"+std::to_string(size)+"_inspect.png"));
        saveOrThrow(upscale(d2,320/size),outDir/("macro_02_"+std::to_string(size)+"_inspect.png"));
    }
    std::cout<<"Macro candidates (40/80/120, both raws) written to "<<outDir.string()<<"\n";

    // ---- Seamless pass: toroidal offset + minimal center-seam repair, on the two
    // main candidate sizes (80 primary, 120 secondary). 40 is kept above only as a
    // size reference point, not carried through seaming - it repeats the same major
    // features too often to add anything the 80 candidate doesn't already show. ----
    std::ostringstream seamReport;
    seamReport<<"# Seam metrics (avg abs channel delta 0-255, self-tile)\n\n"
              <<"`crop edges` = the raw crop's own left/right + top/bottom (why an offset is needed at all).\n"
              <<"`offset edges, pre-repair` = same pixels after the toroidal shift moved them away from the\n"
              <<"crop's original border (these were contiguous interior pixels in the source, so they should\n"
              <<"already be reasonably continuous on their own). `post-repair` = after the center-cross touch-up -\n"
              <<"expected to be near-identical to pre-repair, since the repair band never reaches the outer edge;\n"
              <<"it's reported to confirm the repair didn't accidentally disturb it.\n\n";
    std::map<std::string,sf::Image> seamless; // key: "01_80", "02_80", "01_120", "02_120"
    for(unsigned size:{80u,120u}) {
        for(const auto& [tag,raw,palette] : std::vector<std::tuple<std::string,const sf::Image*,const std::vector<Rgb>*>>{
                {"01",&raw1,&palette1},{"02",&raw2,&palette2}}) {
            const auto crop=quantize(downsampleNearest(*raw,size,size),*palette);
            const double cropH=edgeDiff(crop,crop,true),cropV=edgeDiff(crop,crop,false);
            const auto offset=toroidalOffset(crop);
            const double preH=edgeDiff(offset,offset,true),preV=edgeDiff(offset,offset,false);
            const auto repaired=repairCenterSeam(offset,std::max<unsigned>(2,size/16),*palette);
            const double postH=edgeDiff(repaired,repaired,true),postV=edgeDiff(repaired,repaired,false);
            const std::string key=tag+"_"+std::to_string(size);
            seamless[key]=repaired;
            saveOrThrow(repaired,outDir/("macro_"+key+"_seamless.png"));
            saveOrThrow(upscale(repaired,std::max(1u,320/size)),outDir/("macro_"+key+"_seamless_inspect.png"));
            seamReport<<"- "<<key<<": crop edges h="<<cropH<<" v="<<cropV
                      <<"  |  offset edges pre-repair h="<<preH<<" v="<<preV
                      <<"  |  post-repair h="<<postH<<" v="<<postV<<"\n";
        }
    }
    std::cout<<seamReport.str();
    std::ofstream seamFile(outDir/"seam_metrics.md");
    seamFile<<seamReport.str();

    // ---- Candidate D: quiet base + macro decoration, built from the 80px seamless
    // macro (primary candidate). ----
    const auto compositeD01=quietBaseComposite(seamless["01_80"],palette1);
    const auto compositeD02=quietBaseComposite(seamless["02_80"],palette2);
    saveOrThrow(compositeD01,outDir/"composite_D_01_80.png");
    saveOrThrow(upscale(compositeD01,4),outDir/"composite_D_01_80_inspect.png");
    saveOrThrow(compositeD02,outDir/"composite_D_02_80.png");

    // ---- Pure repeat comparison, same canvas size/zoom/area for every candidate:
    // 240x240 divides evenly by 20, 80 AND 120, so A/B80/B120/D tile without any
    // partial edge tile skewing the comparison. ----
    constexpr unsigned CanvasSize=240;
    sf::Image existingInner01;
    const bool hasInner01=existingInner01.loadFromFile(root/"assets/tiles/avatar_lake/normalized/rock_inner_01.png");
    const fs::path repeatDir=outDir/"repeat_comparison";
    fs::create_directories(repeatDir);
    auto emit=[&](const sf::Image& tile,const std::string& name) {
        const auto plain=tileRepeat(tile,CanvasSize);
        saveOrThrow(plain,repeatDir/(name+".png"));
        saveOrThrow(upscale(plain,2),repeatDir/(name+"_x2.png"));
        const auto grid=withGrid(plain,20);
        saveOrThrow(grid,repeatDir/(name+"_grid.png"));
        saveOrThrow(upscale(grid,2),repeatDir/(name+"_grid_x2.png"));
    };
    if(hasInner01) emit(existingInner01,"A_current_20x20");
    emit(seamless["01_80"],"B_macro_80_shapepreserving");
    emit(seamless["01_120"],"B_macro_120_shapepreserving");
    emit(compositeD01,"D_quietbase_plus_macro");
    // Sanity check: does the offset+repair actually beat just tiling the straight,
    // untouched crop directly? The crop's own natural edge diff (see seam_metrics.md)
    // was already low, so this checks whether the repair's blur artifact is a net loss.
    emit(quantize(downsampleNearest(raw1,80,80),palette1),"E_plain_crop_no_offset_no_repair");
    std::cout<<"Repeat comparison (A/B80/B120/D, grid + no-grid, common "<<CanvasSize<<"x"<<CanvasSize<<" canvas) written to "<<repeatDir.string()<<"\n";

    // ---- Near-real-screen composite mockup: real distant/middle backgrounds, one
    // actual large solid from assets/regions/avatar_lake.json filled with candidate B,
    // a bright diagnostic border standing in for the not-yet-made edge roles, and the
    // proto_one reference sprite for scale. Diagnostic only - not final art. ----
    sf::Image distant,middle,protoRef;
    const bool hasDistant=distant.loadFromFile(root/"assets/backgrounds/avatar_lake/distant.png");
    const bool hasMiddle=middle.loadFromFile(root/"assets/backgrounds/avatar_lake/middle.png");
    // The idle frame (proper alpha-cut sprite), not reference/proto_one.png - that file
    // is a 1254x1254 opaque turnaround sheet, not a game sprite, and would just paint
    // a solid block.
    const bool hasProto=protoRef.loadFromFile(root/"assets/characters/proto_one/normalized/idle/proto_one_idle_00.png");

    // Pick one real Avatar Lake solid to stand in for "a representative big platform" -
    // not an invented rectangle. Skip the tall vertical cliff-face solids (height>300)
    // and take the largest remaining one by area.
    int platformW=250,platformH=120; // fallback if the region file can't be read
    {
        std::ifstream regionFile(root/"assets/regions/avatar_lake.json");
        if(regionFile) {
            const auto regionJson=nlohmann::json::parse(regionFile);
            long bestArea=-1;
            // Pick the largest solid within a size range that actually reads as "one
            // screen-sized platform" (Avatar Lake's real solids range from small ledges
            // up to a 9600-wide ground plane - too wide for one 1920px mockup frame).
            for(const auto& s:regionJson.at("solids")) {
                const int w=s.at(2).get<int>(),h=s.at(3).get<int>();
                if(h>300||w>900) continue;
                if(static_cast<long>(w)*h>bestArea) { bestArea=static_cast<long>(w)*h; platformW=w; platformH=h; }
            }
        }
    }
    if(hasDistant&&hasMiddle) {
        constexpr unsigned SW=1920,SH=1080;
        sf::Image scene({SW,SH},sf::Color(18,22,34,255));
        auto putPixel=[&](int x,int y,sf::Color c) {
            if(x>=0&&y>=0&&x<static_cast<int>(SW)&&y<static_cast<int>(SH))
                scene.setPixel({static_cast<unsigned>(x),static_cast<unsigned>(y)},c);
        };
        auto blit=[&](const sf::Image& layer,int offsetX,int offsetY) {
            const auto s=layer.getSize();
            for(unsigned y=0;y<s.y;++y) for(unsigned x=0;x<s.x;++x) {
                const auto p=layer.getPixel({x,y});
                if(p.a>10) putPixel(static_cast<int>(x)+offsetX,static_cast<int>(y)+offsetY,p);
            }
        };
        // distant/middle source images are already wider than 1920 - center them and
        // anchor near the bottom so the horizon sits roughly where a platform would.
        blit(distant,(static_cast<int>(SW)-static_cast<int>(distant.getSize().x))/2,SH-static_cast<int>(distant.getSize().y)-40);
        blit(middle,(static_cast<int>(SW)-static_cast<int>(middle.getSize().x))/2,SH-static_cast<int>(middle.getSize().y)+60);

        // Real Avatar Lake platform footprint (see above), 1 world unit = 1px (matches
        // tileSize usage elsewhere in this codebase), placed near the bottom of frame.
        const int platformX=(static_cast<int>(SW)-platformW)/2,platformY=SH-260;
        const auto macroTile=seamless["01_80"];
        for(int y=0;y<platformH;++y) for(int x=0;x<platformW;++x) {
            const auto c=macroTile.getPixel({static_cast<unsigned>(x)%80u,static_cast<unsigned>(y)%80u});
            putPixel(platformX+x,platformY+y,c);
        }
        const sf::Color diagnosticBorder(255,0,180,255); // loud on purpose: not final art, marks the missing edge/corner roles
        for(int x=0;x<platformW;++x) { putPixel(platformX+x,platformY,diagnosticBorder); putPixel(platformX+x,platformY+platformH-1,diagnosticBorder); }
        for(int y=0;y<platformH;++y) { putPixel(platformX,platformY+y,diagnosticBorder); putPixel(platformX+platformW-1,platformY+y,diagnosticBorder); }
        if(hasProto) {
            // Manifest pivot for proto_one is (64,123) on its 128x128 canvas - align
            // that point with the platform's top edge so the character reads as
            // standing on it, not floating or sunk in.
            const int protoX=platformX+platformW/2-64,protoY=platformY-123;
            const auto ps=protoRef.getSize();
            for(unsigned y=0;y<ps.y&&y<128;++y) for(unsigned x=0;x<ps.x&&x<128;++x) {
                const auto p=protoRef.getPixel({x,y});
                if(p.a>10) { const int dx=protoX+static_cast<int>(x),dy=protoY+static_cast<int>(y);
                             if(dx>=0&&dy>=0&&dx<static_cast<int>(SW)&&dy<static_cast<int>(SH)) scene.setPixel({static_cast<unsigned>(dx),static_cast<unsigned>(dy)},p); }
            }
        }
        saveOrThrow(scene,outDir/"scene_mockup_1920x1080.png");
        std::cout<<"Near-real scene mockup written (distant="<<hasDistant<<" middle="<<hasMiddle<<" proto_one="<<hasProto<<")\n";
    } else {
        std::cerr<<"WARNING: could not load background layers, skipped scene mockup\n";
    }
}
catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<"\n"; return 1; }
