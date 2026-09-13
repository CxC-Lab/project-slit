// Offline fixed-crop pipeline. No runtime code or asset promotion.
// Usage: tile_pipeline <manifest.json> <new-output-directory>
// Paths in the manifest are relative to its directory. References are optional;
// when present, decoded RGBA equality is required before any output is written.
#include <SFML/Graphics/Image.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs = std::filesystem;
using Json = nlohmann::json;
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

std::vector<Rgb> extractPalette(const std::vector<Rgb>& samples,std::size_t k,unsigned iterations)
{
    auto sorted=samples;
    std::sort(sorted.begin(),sorted.end(),[](auto a,auto b){return a.r+a.g+a.b<b.r+b.g+b.b;});
    std::vector<Rgb> centers(k);
    for(std::size_t i=0;i<k;++i) { const double pct=(i+0.5)/k; centers[i]=sorted[static_cast<std::size_t>(pct*(sorted.size()-1))]; }
    for(unsigned iter=0;iter<iterations;++iter) {
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
sf::Image cropAndReduce(const sf::Image& src,unsigned cx,unsigned cy,unsigned cw,unsigned ch,sf::Vector2u dst,const std::vector<Rgb>& palette)
{
    sf::Image out(dst,sf::Color::Black);
    for(unsigned y=0;y<dst.y;++y) for(unsigned x=0;x<dst.x;++x) {
        const unsigned sx=cx+std::min(cw-1,static_cast<unsigned>((x+0.5)*cw/dst.x));
        const unsigned sy=cy+std::min(ch-1,static_cast<unsigned>((y+0.5)*ch/dst.y));
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

unsigned integer(const Json& value,unsigned minimum,unsigned maximum)
{
    if(!value.is_number_integer() || value.get<double>()<minimum || value.get<double>()>maximum)
        throw std::runtime_error("Expected integer in ["+std::to_string(minimum)+","+std::to_string(maximum)+"]");
    return value.get<unsigned>();
}
void arraySize(const Json& value,std::size_t size)
{
    if(!value.is_array() || value.size()!=size) throw std::runtime_error("Wrong array size");
}
sf::Vector2u dimensions(const Json& value)
{
    arraySize(value,2);
    sf::Vector2u size{integer(value[0],1,16384),integer(value[1],1,16384)};
    if(std::uint64_t(size.x)*size.y>16777216) throw std::runtime_error("Image exceeds 16M pixel safety limit");
    return size;
}
fs::path resolved(const fs::path& path) { return fs::weakly_canonical(fs::absolute(path)); }
std::string pathKey(const fs::path& path)
{
    auto key=resolved(path).generic_string();
#ifdef _WIN32
    std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
#endif
    return key;
}
bool within(const fs::path& child,const fs::path& parent)
{
    auto a=pathKey(child),b=pathKey(parent);
    if(a==b) return true;
    if(b.back()!='/') b+='/';
    return a.starts_with(b);
}
std::vector<std::uint8_t> readBytes(const fs::path& path)
{
    std::ifstream file(path,std::ios::binary);
    if(!file) throw std::runtime_error("Cannot read: "+path.string());
    return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
// Exclusive creation also protects against a file appearing after preflight.
void writeNew(const fs::path& path,const std::vector<std::uint8_t>& bytes)
{
#ifdef _WIN32
    auto* file=_wfopen(path.c_str(),L"wbx");
#else
    auto* file=std::fopen(path.c_str(),"wbx");
#endif
    if(!file) throw std::runtime_error("Exclusive create failed: "+path.string());
    const auto written=std::fwrite(bytes.data(),1,bytes.size(),file);
    const auto closeResult=std::fclose(file);
    if(written!=bytes.size() || closeResult!=0) throw std::runtime_error("Write failed: "+path.string());
}
struct Candidate {
    std::string id;
    unsigned x,y,w,h;
    sf::Vector2u size;
    fs::path reference;
};
} // namespace

int main(int argc,char** argv) try
{
    if(argc==4 && std::string(argv[1])=="--copy-new") {
        const auto to=resolved(argv[3]);
        if(to.parent_path().filename()!="runtime") throw std::runtime_error("Export requires runtime/ directory");
        for(const auto& part:to) {
            auto key=part.string();
            std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
            if(key=="source" || key=="candidates") throw std::runtime_error("Protected export directory");
        }
        writeNew(argv[3],readBytes(argv[2]));
        return 0;
    }
    if(argc==4 && std::string(argv[1])=="--compare-images") {
        sf::Image a,b;
        if(!a.loadFromFile(argv[2]) || !b.loadFromFile(argv[3])) throw std::runtime_error("Comparison image load failed");
        if(a.getSize()!=b.getSize()) throw std::runtime_error("Comparison dimensions differ");
        std::size_t differences=0;
        for(unsigned y=0;y<a.getSize().y;++y) for(unsigned x=0;x<a.getSize().x;++x)
            differences+=a.getPixel({x,y})!=b.getPixel({x,y});
        std::cout<<"different_rgba_pixels="<<differences<<'\n';
        return differences==0 ? 0 : 1;
    }
    if(argc!=3) throw std::runtime_error("Usage: tile_pipeline <manifest.json> <new-output-directory>");
    const auto manifestPath=resolved(argv[1]),base=manifestPath.parent_path(),out=resolved(argv[2]);
    std::ifstream file(manifestPath);
    if(!file) throw std::runtime_error("Cannot open manifest: "+manifestPath.string());
    const auto manifest=Json::parse(file);
    if(integer(manifest.at("schema"),1,1)!=1) throw std::runtime_error("Unsupported schema");
    const auto source=resolved(base/manifest.at("source").at("path").get<std::string>());
    const auto expected=dimensions(manifest.at("source").at("expected_size"));
    const auto& p=manifest.at("palette");
    const auto colors=integer(p.at("colors"),1,256),iterations=integer(p.at("iterations"),1,100);
    const std::map<std::string,std::string> policy{
        {"method","legacy_rgb_kmeans_v1"},{"sampling","all_source_pixels_row_major"},
        {"initialization","brightness_sum_std_sort_percentiles"},{"mean","int_truncate"},
        {"distance","squared_rgb_first_tie"},{"sort","brightness_sum"}};
    for(const auto& [key,value]:policy)
        if(p.at(key)!=value) throw std::runtime_error("Unsupported palette setting: "+key);
    if(manifest.at("stages")!=Json({"fixed_crop","nearest_point_center","nearest_palette_rgb","opaque_rgba"}))
        throw std::runtime_error("Unsupported stages/order");
    const auto& preview=manifest.at("previews");
    const auto repeat=dimensions(preview.at("repeat"));
    const auto zoom=integer(preview.at("inspect_scale"),1,64);
    const auto& rgba=preview.at("grid_rgba"); arraySize(rgba,4);
    const sf::Color gridColor(static_cast<std::uint8_t>(integer(rgba[0],0,255)),
        static_cast<std::uint8_t>(integer(rgba[1],0,255)),static_cast<std::uint8_t>(integer(rgba[2],0,255)),
        static_cast<std::uint8_t>(integer(rgba[3],0,255)));
    const auto& entries=manifest.at("candidates");
    if(!entries.is_array() || entries.empty()) throw std::runtime_error("Candidates must be a nonempty array");
    std::vector<Candidate> candidates;
    std::set<std::string> ids;
    // Protect the entire manifest asset directory, even if references are omitted.
    std::vector<fs::path> protectedDirs{base,source.parent_path()};
    for(const auto& entry:entries) {
        const auto id=entry.at("id").get<std::string>();
        if(id.empty() || !std::all_of(id.begin(),id.end(),[](unsigned char c){return
            (c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-';}))
            throw std::runtime_error("Candidate id must contain only ASCII letters, digits, _ or -");
        auto folded=id; std::transform(folded.begin(),folded.end(),folded.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        if(!ids.insert(folded).second) throw std::runtime_error("Duplicate candidate id: "+id);
        const auto& crop=entry.at("crop"); arraySize(crop,4);
        Candidate c{id,integer(crop[0],0,16384),integer(crop[1],0,16384),
            integer(crop[2],1,16384),integer(crop[3],1,16384),dimensions(entry.at("output_size")),{}};
        if(c.x+c.w>expected.x || c.y+c.h>expected.y) throw std::runtime_error("Crop outside source: "+id);
        dimensions(Json({std::uint64_t(c.size.x)*repeat.x,std::uint64_t(c.size.y)*repeat.y}));
        dimensions(Json({std::uint64_t(c.size.x)*zoom,std::uint64_t(c.size.y)*zoom}));
        if(entry.contains("reference")) {
            c.reference=resolved(base/entry.at("reference").get<std::string>());
            protectedDirs.push_back(c.reference.parent_path());
        }
        candidates.push_back(c);
    }
    for(const auto& dir:protectedDirs)
        if(within(out,dir) || within(dir,out)) throw std::runtime_error("Output overlaps protected input/reference directory: "+out.string());
    sf::Image raw;
    if(!raw.loadFromFile(source)) throw std::runtime_error("Cannot load source: "+source.string());
    if(raw.getSize()!=expected) throw std::runtime_error("Source size does not match expected_size");
    const auto palette=extractPalette(allPixels(raw),colors,iterations);
    Json report{{"manifest",manifest},{"palette_rgb",Json::array()},{"candidates",Json::array()}};
    for(const auto& color:palette) report["palette_rgb"].push_back({color.r,color.g,color.b});
    // Resolve snapshot paths so report.manifest can be saved anywhere and reused.
    report["manifest"]["source"]["path"]=source.generic_string();
    std::map<std::string,std::vector<std::uint8_t>> outputs;
    auto addImage=[&](const std::string& name,const sf::Image& image) {
        auto bytes=image.saveToMemory("png");
        if(!bytes) throw std::runtime_error("PNG encoding failed: "+name);
        if(!outputs.emplace(name,std::move(*bytes)).second) throw std::runtime_error("Output name collision: "+name);
    };
    bool mismatched=false;
    for(std::size_t index=0;index<candidates.size();++index) {
        const auto& c=candidates[index];
        const auto image=cropAndReduce(raw,c.x,c.y,c.w,c.h,c.size,palette);
        Json result{{"id",c.id},{"edge_h",edgeDiff(image,true)},{"edge_v",edgeDiff(image,false)}};
        if(!c.reference.empty()) {
            sf::Image reference;
            if(!reference.loadFromFile(c.reference)) throw std::runtime_error("Cannot load reference: "+c.reference.string());
            if(reference.getSize()!=c.size) throw std::runtime_error("Reference size mismatch: "+c.id);
            std::size_t differences=0;
            for(unsigned y=0;y<c.size.y;++y) for(unsigned x=0;x<c.size.x;++x)
                differences+=image.getPixel({x,y})!=reference.getPixel({x,y});
            result["different_rgba_pixels"]=differences;
            mismatched|=differences!=0;
            report["manifest"]["candidates"][index]["reference"]=c.reference.generic_string();
        }
        std::cout<<result.dump()<<'\n';
        report["candidates"].push_back(result);
        addImage(c.id+".png",image);
        sf::Image inspection({c.size.x*zoom,c.size.y*zoom},sf::Color::Black);
        for(unsigned y=0;y<inspection.getSize().y;++y) for(unsigned x=0;x<inspection.getSize().x;++x)
            inspection.setPixel({x,y},image.getPixel({x/zoom,y/zoom}));
        addImage(c.id+"_inspect.png",inspection);
        sf::Image repeated({c.size.x*repeat.x,c.size.y*repeat.y},sf::Color::Black);
        for(unsigned y=0;y<repeated.getSize().y;++y) for(unsigned x=0;x<repeated.getSize().x;++x)
            repeated.setPixel({x,y},image.getPixel({x%c.size.x,y%c.size.y}));
        addImage(c.id+"_repeat.png",repeated);
        for(unsigned y=0;y<repeated.getSize().y;++y) for(unsigned x=0;x<repeated.getSize().x;++x)
            if(x%c.size.x==0 || y%c.size.y==0) repeated.setPixel({x,y},gridColor);
        addImage(c.id+"_grid.png",repeated);
    }
    if(mismatched) throw std::runtime_error("Reference RGBA mismatch; no output written (see per-candidate counts)");
    const auto reportText=report.dump(2)+"\n";
    outputs.emplace("report.json",std::vector<std::uint8_t>(reportText.begin(),reportText.end()));
    // Preflight every file before writing any; identical results are left untouched.
    for(const auto& [name,bytes]:outputs) {
        const auto path=out/name;
        if(!within(path,out)) throw std::runtime_error("Output path escapes directory: "+path.string());
        if(fs::is_symlink(fs::symlink_status(path))) throw std::runtime_error("Output symlink forbidden: "+path.string());
        if(fs::exists(path) && (!fs::is_regular_file(path) || readBytes(path)!=bytes))
            throw std::runtime_error("Refusing to overwrite different existing content: "+path.string());
    }
    fs::create_directories(out);
    for(const auto& [name,bytes]:outputs) if(!fs::exists(out/name)) writeNew(out/name,bytes);
    std::cout<<"OK: "<<outputs.size()<<" files generated or identical in "<<out.string()<<'\n';
    return 0;
}
catch(const std::exception& error) { std::cerr<<"FAIL: "<<error.what()<<'\n'; return 1; }
