// Offline schema-2 boundary normalization. No runtime rendering or export.
#include "PipelineFiles.hpp"
#include <SFML/Graphics/Image.hpp>
#include <nlohmann/json.hpp>
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <limits>
#include <numeric>
#include <sstream>
#include <tuple>
namespace fs=std::filesystem;
using J=nlohmann::json;
using namespace PipelineFiles;
using Bytes=std::vector<std::uint8_t>;
using Outputs=std::map<std::string,Bytes>;
void require(bool value,const std::string& why) {if(!value) throw std::runtime_error(why);}
unsigned number(const J& v,unsigned low=0,unsigned high=16384) {
    require(v.is_number_integer() && v.get<double>()>=low && v.get<double>()<=high,"Integer out of range");return v.get<unsigned>();
}
double fraction(const J& v) {const double x=v.get<double>();require(std::isfinite(x)&&x>=0&&x<=1,"Invalid fraction");return x;}
sf::Vector2u sizeOf(const J& j) {
    require(j.is_array()&&j.size()==2,"Expected dimensions");return {number(j[0],1),number(j[1],1)};
}
sf::Color color(const J& j) {
    require(j.is_array()&&j.size()==3,"Expected RGB");return {static_cast<std::uint8_t>(number(j[0],0,255)),static_cast<std::uint8_t>(number(j[1],0,255)),static_cast<std::uint8_t>(number(j[2],0,255))};
}
J rgb(sf::Color c) {return J::array({c.r,c.g,c.b});}
int brightness(sf::Color c) {return 299*c.r+587*c.g+114*c.b;}
double magenta(sf::Color c) {return (c.r+c.b)/2.0-c.g;}
double distance2(sf::Color a,sf::Color b) {const int r=int(a.r)-b.r,g=int(a.g)-b.g,bl=int(a.b)-b.b;return r*r+g*g+bl*bl;}
bool verticalRepeat(const J& cfg) {
    if(!cfg.contains("orientation"))return false; // Legacy north-facing manifests stay byte-compatible.
    const auto& o=cfg.at("orientation");const auto depth=o.at("depth_axis"),repeat=o.at("repeat_axis");
    require((depth=="x"&&repeat=="y"&&o.at("exposure")=="W")||
            (depth=="y"&&repeat=="x"&&o.at("exposure")=="N"),"Unsupported orientation");
    require(o.at("seam_axis")==repeat&&o.at("opacity_axis")==depth,"Inconsistent inspection axes");
    return repeat=="y";
}
sf::Image transpose(const sf::Image& image) {
    const auto size=image.getSize();sf::Image result(sf::Vector2u{size.y,size.x});
    for(unsigned y=0;y<size.y;++y)for(unsigned x=0;x<size.x;++x)result.setPixel({y,x},image.getPixel({x,y}));
    return result;
}
std::string sha256(Bytes data) {
    require(data.size()<=std::numeric_limits<ULONG>::max(),"Hash input too large");
    BCRYPT_ALG_HANDLE algorithm{};
    require(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,"SHA256 provider failed");
    std::array<unsigned char,32> hash{};
    const auto status=BCryptHash(algorithm,nullptr,0,data.data(),static_cast<ULONG>(data.size()),hash.data(),static_cast<ULONG>(hash.size()));
    BCryptCloseAlgorithmProvider(algorithm,0);require(status>=0,"SHA256 failed");
    std::ostringstream out;out<<std::hex<<std::setfill('0');for(auto b:hash) out<<std::setw(2)<<int(b);return out.str();
}
void png(Outputs& out,const std::string& name,const sf::Image& image) {
    auto data=image.saveToMemory("png");require(data.has_value(),"PNG encoding failed");
    require(out.emplace(name,std::move(*data)).second,"Duplicate output name");
}
void json(Outputs& out,const std::string& name,const J& value) {auto s=value.dump(2)+'\n';require(out.emplace(name,Bytes(s.begin(),s.end())).second,"Duplicate output name");}
void publish(const fs::path& out,const Outputs& files) {
    for(const auto& [name,bytes]:files) {
        const auto to=out/name;require(!fs::is_symlink(to),"Output symlink refused");
        if(fs::exists(to)) require(readBytes(to)==bytes,"Refusing to overwrite different output: "+to.string());
    }
    fs::create_directories(out);
    for(const auto& [name,bytes]:files) if(!fs::exists(out/name)) writeNew(out/name,bytes);
}
void protect(const fs::path& out,const fs::path& manifest,const J& cfg) {
    std::vector<fs::path> inputs{manifest.parent_path(),resolved(manifest.parent_path()/cfg.at("source").at("path").get<std::string>()).parent_path(),
        resolved(manifest.parent_path()/cfg.at("previews").at("inner").get<std::string>()).parent_path()};
    if(cfg.contains("references")) for(const auto& path:cfg.at("references")) inputs.push_back(resolved(manifest.parent_path()/path.get<std::string>()).parent_path());
    for(const auto& part:out) {auto key=part.string();std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return char(std::tolower(c));});
        require(key!="runtime"&&key!="candidates"&&key!="source","Protected output directory");}
    for(const auto& input:inputs) require(!within(out,input)&&!within(input,out),"Output overlaps protected input/reference directory");
}
struct Crop {unsigned x,y,w,h;};
Crop cropOf(const J& j,sf::Vector2u source,sf::Vector2u output) {
    require(j.is_array()&&j.size()==4,"Expected crop");Crop c{number(j[0]),number(j[1]),number(j[2],1),number(j[3],1)};
    require(c.x+c.w<=source.x&&c.y+c.h<=source.y,"Crop outside source");
    require(std::uint64_t(c.w)*output.y==std::uint64_t(c.h)*output.x,"Crop must have uniform scale");return c;
}
void clean(Bytes& mask,unsigned w,unsigned h,const J& cfg) {
    require(number(cfg.at("connectivity"))==4,"Only four-connected cleanup supported");
    for(bool foreground:{true,false}) {
        const unsigned minimum=number(cfg.at(foreground?"min_component_pixels":"min_hole_pixels"),0,16777216);
        if(minimum==0) continue;
        Bytes seen(mask.size());
        for(unsigned start=0;start<mask.size();++start) if(!seen[start]&&bool(mask[start])==foreground) {
            std::vector<unsigned> component{start};seen[start]=1;bool edge=false;
            for(std::size_t i=0;i<component.size();++i) {
                const unsigned at=component[i],x=at%w,y=at/w;edge|=x==0||y==0||x+1==w||y+1==h;
                const auto visit=[&](unsigned next) {if(!seen[next]&&bool(mask[next])==foreground){seen[next]=1;component.push_back(next);}};
                if(x)visit(at-1);if(x+1<w)visit(at+1);if(y)visit(at-w);if(y+1<h)visit(at+w);
            }
            if(component.size()<minimum&&(foreground||!edge)) for(auto at:component)mask[at]=!foreground;
        }
    }
}
J distribution(std::vector<double> values) {
    J out{{"count",values.size()}};if(values.empty())return out;
    std::sort(values.begin(),values.end());out.update({{"min",values.front()},{"max",values.back()},
        {"median",(values[(values.size()-1)/2]+values[values.size()/2])/2},{"p95",values[values.size()*95/100]}});return out;
}
std::vector<unsigned> backgroundDistances(const Bytes& mask,unsigned w,unsigned h) {
    std::vector<unsigned> distances(mask.size(),std::numeric_limits<unsigned>::max()),queue;
    for(unsigned i=0;i<mask.size();++i)if(!mask[i]){distances[i]=0;queue.push_back(i);}
    // Only actual background inside the crop counts; crop borders do not imply background.
    for(std::size_t i=0;i<queue.size();++i){const auto at=queue[i],x=at%w,y=at/w;
        const auto visit=[&](unsigned next){if(distances[next]>distances[at]+1){distances[next]=distances[at]+1;queue.push_back(next);}};
        if(x)visit(at-1);if(x+1<w)visit(at+1);if(y)visit(at-w);if(y+1<h)visit(at+w);
    }return distances;
}
J removeFringe(const sf::Image& crop,Bytes& mask,Bytes& removed,const J& rule) {
    require(rule.at("distance")=="manhattan4","Unsupported fringe distance");
    const auto depth=number(rule.at("depth"),1,32),iterations=number(rule.at("iterations"),1,32);
    const double minimum=rule.at("magenta_min").get<double>();require(std::isfinite(minimum)&&minimum>=-255&&minimum<=255,"Invalid fringe index");
    const auto size=crop.getSize();removed.assign(mask.size(),0);
    auto distances=backgroundDistances(mask,size.x,size.y);std::vector<double> edge,interior,indices;
    for(unsigned i=0;i<mask.size();++i)if(mask[i])(distances[i]<=depth?edge:interior).push_back(magenta(crop.getPixel({i%size.x,i/size.x})));
    std::vector<unsigned> rows(size.y),perIteration;
    for(unsigned iteration=0;iteration<iterations;++iteration){
        if(iteration)distances=backgroundDistances(mask,size.x,size.y);
        std::vector<unsigned> pending;
        for(unsigned i=0;i<mask.size();++i)if(mask[i]&&distances[i]<=depth&&magenta(crop.getPixel({i%size.x,i/size.x}))>=minimum)pending.push_back(i);
        perIteration.push_back(static_cast<unsigned>(pending.size()));
        for(auto i:pending){mask[i]=0;removed[i]=1;++rows[i/size.x];indices.push_back(magenta(crop.getPixel({i%size.x,i/size.x})));}
        if(pending.empty())break;
    }
    return {{"rule",rule},{"edge_magenta",distribution(edge)},{"interior_magenta",distribution(interior)},
        {"removed_magenta",distribution(indices)},{"removed_source_pixels",indices.size()},{"removed_source_rows",rows},{"removed_per_iteration",perIteration}};
}
std::string classify(const J& checks,const J& limits) {
    const auto holds=limits.value("art_hold_checks",J::array());require(holds.is_array(),"Expected art hold list");
    for(const auto& name:holds)require(name.is_string()&&checks.contains(name.get<std::string>()),"Unknown art hold check");
    bool held=false;
    for(const auto& [name,passed]:checks.items())if(!passed.get<bool>()){
        if(std::find(holds.begin(),holds.end(),J(name))==holds.end())return "FAIL";held=true;
    }return held?"ART_HOLD":"PASS";
}
struct Segmented {sf::Image crop;Bytes mask;sf::Image sampled;J stats;Bytes removed;};
Segmented segment(const sf::Image& raw,Crop c,const J& cfg,double threshold) {
    const auto output=sizeOf(cfg.at("output_size"));const auto bg=color(cfg.at("background").at("rgb"));
    Segmented s{sf::Image(sf::Vector2u{c.w,c.h}),Bytes(c.w*c.h),sf::Image(output,sf::Color::Transparent),J::object()};
    std::vector<double> noise;unsigned nearCount=0;
    for(unsigned y=0;y<c.h;++y) for(unsigned x=0;x<c.w;++x) {
        const auto p=raw.getPixel({c.x+x,c.y+y});s.crop.setPixel({x,y},p);
        const double d=std::sqrt(distance2(p,bg));s.mask[y*c.w+x]=p.a!=0&&d>threshold;
        if(!s.mask[y*c.w+x])noise.push_back(d);
        else if(d<=2*threshold)++nearCount;
    }
    if(cfg.contains("fringe"))s.stats["fringe"]=removeFringe(s.crop,s.mask,s.removed,cfg.at("fringe"));
    const auto original=s.mask;clean(s.mask,c.w,c.h,cfg.at("mask_cleanup"));
    unsigned changed=0;for(std::size_t i=0;i<s.mask.size();++i)changed+=s.mask[i]!=original[i];
    for(unsigned y=0;y<output.y;++y)for(unsigned x=0;x<output.x;++x) {
        const unsigned sx=std::min(c.w-1,static_cast<unsigned>((x+.5)*c.w/output.x));
        const unsigned sy=std::min(c.h-1,static_cast<unsigned>((y+.5)*c.h/output.y));
        auto p=s.crop.getPixel({sx,sy});p.a=s.mask[sy*c.w+sx]?255:0;if(!p.a)p=sf::Color::Transparent;s.sampled.setPixel({x,y},p);
    }
    std::sort(noise.begin(),noise.end());
    s.stats.update({{"source_near_background_opaque",nearCount},{"cleanup_changed_source_pixels",changed},{"background_noise_count",noise.size()}});
    if(!noise.empty())s.stats["background_noise_distance"]={{"min",noise.front()},{"max",noise.back()},{"median",noise[noise.size()/2]},
        {"p95",noise[std::min(noise.size()-1,noise.size()*95/100)]},{"mean",std::accumulate(noise.begin(),noise.end(),0.0)/noise.size()}};
    return s;
}
J seams(const sf::Image& image) {
    auto size=image.getSize();double sum=0,seam=0;unsigned pairs=0,seamPairs=0,alpha=0,seamAlpha=0;
    for(unsigned y=0;y<size.y;++y)for(unsigned x=0;x<size.x;++x) {
        const auto a=image.getPixel({x,y}),b=image.getPixel({(x+1)%size.x,y});const bool boundary=x+1==size.x;
        (boundary?seamAlpha:alpha)+=(a.a!=0)!=(b.a!=0);
        if(a.a&&b.a) {const double diff=(std::abs(int(a.r)-b.r)+std::abs(int(a.g)-b.g)+std::abs(int(a.b)-b.b))/3.0;
            if(boundary){seam+=diff;++seamPairs;}else{sum+=diff;++pairs;}}
    }
    const double internal=pairs?sum/pairs:0,edge=seamPairs?seam/seamPairs:0;
    const double ratio=internal>0?edge/internal:(edge==0?0:1e9);
    return {{"color_seam_ratio",ratio},{"color_edge_mean",edge},{"color_internal_mean",internal},{"color_edge_pairs",seamPairs},
        {"alpha_edge_mismatches",seamAlpha},{"alpha_internal_mean",double(alpha)/(size.x-1)}};
}
std::vector<sf::Color> palette(const Segmented& s,const J& cfg) {
    const auto& p=cfg.at("palette");const unsigned k=number(p.at("colors"),1,10),iterations=number(p.at("iterations"),1,100);
    std::vector<sf::Color> samples;for(unsigned y=0;y<s.crop.getSize().y;++y)for(unsigned x=0;x<s.crop.getSize().x;++x)
        if(s.mask[y*s.crop.getSize().x+x]) samples.push_back(s.crop.getPixel({x,y}));
    require(samples.size()>=k,"Insufficient opaque palette samples");
    auto ordered=samples;std::stable_sort(ordered.begin(),ordered.end(),[](auto a,auto b){return brightness(a)<brightness(b);});
    std::vector<sf::Color> colors;for(unsigned i=0;i<k;++i)colors.push_back(ordered[(2*i+1)*ordered.size()/(2*k)]);
    for(unsigned iter=0;iter<iterations;++iter) {
        std::vector<std::array<std::uint64_t,4>> sums(k);
        for(auto pixel:samples) {unsigned best=0;for(unsigned i=1;i<k;++i)if(distance2(pixel,colors[i])<distance2(pixel,colors[best]))best=i;
            auto& sum=sums[best];sum[0]+=pixel.r;sum[1]+=pixel.g;sum[2]+=pixel.b;++sum[3];}
        for(unsigned i=0;i<k;++i)if(sums[i][3])colors[i]={static_cast<std::uint8_t>(sums[i][0]/sums[i][3]),static_cast<std::uint8_t>(sums[i][1]/sums[i][3]),static_cast<std::uint8_t>(sums[i][2]/sums[i][3])};
    }
    std::stable_sort(colors.begin(),colors.end(),[](auto a,auto b){return brightness(a)<brightness(b);});return colors;
}
sf::Image quantize(const sf::Image& sampled,const std::vector<sf::Color>& palette) {
    auto out=sampled;for(unsigned y=0;y<out.getSize().y;++y)for(unsigned x=0;x<out.getSize().x;++x) {
        auto p=out.getPixel({x,y});if(!p.a)continue;auto nearest=palette.front();
        for(auto c:palette)if(distance2(p,c)<distance2(p,nearest))nearest=c;nearest.a=255;out.setPixel({x,y},nearest);
    }return out;
}
J inspectCanonical(const sf::Image& image,const sf::Image& sampled,const J& cfg) {
    const auto& limits=cfg.at("checks");const auto size=image.getSize();J stats=seams(image),checks=J::object();
    std::map<std::tuple<int,int,int>,unsigned> counts,first;unsigned opaque=0,partial=0,nearCount=0,fringe=0;
    std::vector<double> rows;const auto bg=color(cfg.at("background").at("rgb"));
    const double threshold=cfg.at("background").at("threshold").get<double>();
    for(unsigned y=0;y<size.y;++y) {unsigned row=0;for(unsigned x=0;x<size.x;++x) {
        const auto p=image.getPixel({x,y});partial+=p.a!=0&&p.a!=255;if(!p.a)continue;++opaque;++row;
        const auto key=std::make_tuple(p.r,p.g,p.b);++counts[key];if(y==0)++first[key];
        const auto source=sampled.getPixel({x,y});nearCount+=distance2(source,bg)<=4*threshold*threshold;
        bool edge=false;for(const auto delta:std::array<sf::Vector2i,4>{{{-1,0},{1,0},{0,-1},{0,1}}}) {
            const int nx=int(x)+delta.x,ny=int(y)+delta.y;
            // Horizontal neighbors wrap, matching the repeated strip.
            if(ny>=0&&ny<int(size.y)&&!image.getPixel({unsigned((nx+int(size.x))%int(size.x)),unsigned(ny)}).a)edge=true;
        }
        fringe+=edge&&magenta(source)>limits.at("magenta_index_max").get<double>();
    }rows.push_back(double(row)/size.x);}
    std::vector<sf::Color> actual;for(const auto& [key,count]:counts){const auto [r,g,b]=key;actual.emplace_back(r,g,b);}
    std::stable_sort(actual.begin(),actual.end(),[](auto a,auto b){return brightness(a)<brightness(b);});
    unsigned bright=0,maxFirst=0,magentaColors=0,magentaPixels=0;bool bgSafe=true,magentaSafe=true;
    const unsigned top=number(limits.at("bright_top_count"),1,10);J colors=J::array();
    for(std::size_t i=0;i<actual.size();++i){const auto p=actual[i];colors.push_back(rgb(p));
        if(i+top>=actual.size())bright+=counts.at({p.r,p.g,p.b});
        bgSafe&=std::sqrt(distance2(p,bg))>=limits.at("palette_background_min_distance").get<double>();
        const bool bad=magenta(p)>limits.at("magenta_index_max").get<double>();magentaSafe&=!bad;
        if(bad){++magentaColors;magentaPixels+=counts.at({p.r,p.g,p.b});}}
    for(auto [key,count]:first)maxFirst=std::max(maxFirst,count);
    const auto brightFraction=opaque?double(bright)/opaque:1.0;
    const double row0Fraction=rows[0]>0?maxFirst/(rows[0]*size.x):1.0;
    double maxIncrease=0;const auto start=number(limits.at("row_increase_start"),1,size.y-1);
    for(unsigned y=start+1;y<size.y;++y)maxIncrease=std::max(maxIncrease,rows[y]-rows[y-1]);
    checks["size"]=size==sizeOf(limits.at("expected_size"));checks["binary_alpha"]=partial==0;
    checks["palette_count"]=!actual.empty()&&actual.size()<=number(limits.at("max_colors"),1,10);
    checks["row0"]=rows[0]>=fraction(limits.at("row0_min"));checks["last_row"]=rows.back()<=fraction(limits.at("last_row_max"));
    checks["row_progression"]=maxIncrease<=fraction(limits.at("row_increase_max"));checks["bright_fraction"]=brightFraction<=fraction(limits.at("bright_fraction_max"));
    checks["row0_dominant"]=row0Fraction<=fraction(limits.at("row0_dominant_max"));
    checks["color_seam"]=stats.at("color_seam_ratio").get<double>()<=limits.at("color_seam_ratio_max").get<double>();
    checks["alpha_seam"]=stats.at("alpha_edge_mismatches").get<double>()<=stats.at("alpha_internal_mean").get<double>();
    checks["palette_background"]=bgSafe;checks["palette_magenta"]=magentaSafe;
    stats.update({{"size",{size.x,size.y}},{"partial_alpha",partial},{"opaque_pixels",opaque},{"unique_colors",actual.size()},
        {"palette_rgb",colors},{"magenta_palette_colors",magentaColors},{"magenta_output_pixels",magentaPixels},{"row_opacity",rows},{"max_row_increase",maxIncrease},{"bright_fraction",brightFraction},
        {"row0_dominant_fraction",row0Fraction},{"near_background_output_samples",nearCount},{"magenta_fringe_output_samples",fringe},{"checks",checks}});return stats;
}
J inspect(const sf::Image& image,const sf::Image& sampled,const J& cfg) {
    if(!verticalRepeat(cfg))return inspectCanonical(image,sampled,cfg);
    auto canonical=cfg;const auto expected=sizeOf(cfg.at("checks").at("expected_size"));
    canonical["checks"]["expected_size"]={expected.y,expected.x};
    auto result=inspectCanonical(transpose(image),transpose(sampled),canonical);
    result["size"]={image.getSize().x,image.getSize().y};result["opacity_axis"]=cfg.at("orientation").at("opacity_axis");
    result["column_opacity"]=result.at("row_opacity");result.erase("row_opacity");return result;
}
sf::Image scale(const sf::Image& image,unsigned factor) {
    const auto size=image.getSize();require(factor>0&&std::uint64_t(size.x)*size.y*factor*factor<=64000000,"Preview too large");
    sf::Image out(sf::Vector2u{size.x*factor,size.y*factor});for(unsigned y=0;y<out.getSize().y;++y)for(unsigned x=0;x<out.getSize().x;++x)out.setPixel({x,y},image.getPixel({x/factor,y/factor}));return out;
}
J alphaChanges(const sf::Image& before,const sf::Image& after) {
    require(before.getSize()==after.getSize(),"Comparison size mismatch");J removed=J::array(),added=J::array();
    const auto size=before.getSize();
    for(unsigned y=0;y<size.y;++y)for(unsigned x=0;x<size.x;++x){const bool a=before.getPixel({x,y}).a!=0,b=after.getPixel({x,y}).a!=0;
        if(a&&!b)removed.push_back({x,y});if(!a&&b)added.push_back({x,y});}
    return {{"opaque_to_transparent",removed},{"transparent_to_opaque",added},{"count",removed.size()+added.size()},
        {"fraction",double(removed.size()+added.size())/(size.x*size.y)}};
}
sf::Image decoded(const Outputs& out,const std::string& name) {
    const auto& bytes=out.at(name);sf::Image image;require(image.loadFromMemory(bytes.data(),bytes.size()),"Cannot decode comparison image");return image;
}
sf::Image stacked(const sf::Image& before,const sf::Image& after) {
    const auto size=before.getSize();require(size==after.getSize(),"Comparison size mismatch");
    sf::Image result(sf::Vector2u{size.x,size.y*2});require(result.copy(before,{0,0})&&result.copy(after,{0,size.y}),"Comparison copy failed");return result;
}
sf::Image composite(const sf::Image& top,const sf::Image& inner,unsigned width,const J& p,bool marks) {
    if(p.value("layout","floor")=="wall"){
        const auto wall=number(p.at("wall_width"),1),margin=number(p.at("above_rows"),1);
        require(wall>=top.getSize().x*2,"Wall must fit both boundary overlays");
        sf::Image out({wall+2*margin,width},color(p.at("neutral_rgb")));
        for(unsigned y=0;y<width;++y)for(unsigned x=0;x<wall;++x){
            auto pixel=inner.getPixel({x%inner.getSize().x,y%inner.getSize().y});
            if(x<top.getSize().x){const auto edge=top.getPixel({x,y%top.getSize().y});if(edge.a)pixel=edge;}
            if(wall-1-x<top.getSize().x){const auto edge=top.getPixel({wall-1-x,y%top.getSize().y});if(edge.a)pixel=edge;}
            out.setPixel({margin+x,y},pixel);
        }
        if(marks)for(unsigned y=0;y<width;y+=top.getSize().y)for(unsigned n=0;n<number(p.at("marker_rows"),1)&&y+n<width;++n)
            for(unsigned x=0;x<margin;++x){out.setPixel({x,y+n},color(p.at("marker_rgb")));out.setPixel({out.getSize().x-1-x,y+n},color(p.at("marker_rgb")));}
        return out;
    }
    const unsigned above=number(p.at("above_rows"),1),rock=number(p.at("rock_rows"),1),margin=marks?number(p.at("marker_rows"),1):0;
    const auto neutral=color(p.at("neutral_rgb"));sf::Image out({width,above+rock+2*margin},neutral);
    for(unsigned y=0;y<rock;++y)for(unsigned x=0;x<width;++x) {
        auto pixel=inner.getPixel({x%inner.getSize().x,y%inner.getSize().y});
        if(y<top.getSize().y&&top.getPixel({x%top.getSize().x,y}).a)pixel=top.getPixel({x%top.getSize().x,y});out.setPixel({x,margin+above+y},pixel);
    }
    if(marks)for(unsigned x=0;x<width;x+=top.getSize().x)for(unsigned y=0;y<margin;++y){out.setPixel({x,y},color(p.at("marker_rgb")));out.setPixel({x,out.getSize().y-1-y},color(p.at("marker_rgb")));}
    return out;
}
sf::Image previews(Outputs& out,const std::string& id,const Segmented& s,const sf::Image& top,const sf::Image& inner,const J& p) {
    const unsigned zoom=number(p.at("inspect_scale"),1,8),game=number(p.at("game_scale"),1,8);
    png(out,id+"_crop.png",s.crop);auto mask=s.crop;
    for(unsigned y=0;y<mask.getSize().y;++y)for(unsigned x=0;x<mask.getSize().x;++x)if(!s.mask[y*mask.getSize().x+x])mask.setPixel({x,y},color(p.at("mask_rgb")));
    png(out,id+"_mask.png",mask);png(out,id+"_crop_zoom.png",scale(s.crop,zoom));png(out,id+"_mask_zoom.png",scale(mask,zoom));
    auto checker=top;const auto cell=number(p.at("checker_cell"),1);
    for(unsigned y=0;y<top.getSize().y;++y)for(unsigned x=0;x<top.getSize().x;++x)if(!top.getPixel({x,y}).a)checker.setPixel({x,y},color(p.at("checker_rgb")[(x/cell+y/cell)%2]));
    png(out,id+"_checker_zoom.png",scale(checker,zoom));
    const auto period=p.value("layout","floor")=="wall"?top.getSize().y:top.getSize().x;
    for(const auto& repeat:p.at("repeat")){const auto n=number(repeat,1,32);for(bool marked:{false,true})png(out,id+"_repeat_"+std::to_string(n)+(marked?"_marks.png":".png"),composite(top,inner,n*period,p,marked));}
    sf::Image world;for(bool marked:{false,true}){auto image=composite(top,inner,number(p.at("world_width"),1),p,marked);
        png(out,id+(marked?"_world_marks.png":"_world.png"),image);png(out,id+(marked?"_world_marks_game.png":"_world_game.png"),scale(image,game));if(!marked)world=image;}
    return world;
}
void selfTest(const J& cfg) {
    const auto size=sizeOf(cfg.at("output_size"));sf::Image image(size,color(cfg.at("background").at("rgb")));
    auto result=inspect(image,image,cfg);require(!result["checks"]["palette_magenta"].get<bool>(),"Self-test failed: magenta detection");
    auto p=image.getPixel({0,0});p.a=128;image.setPixel({0,0},p);result=inspect(image,image,cfg);
    require(!result["checks"]["binary_alpha"].get<bool>(),"Self-test failed: partial alpha detection");
    sf::Image wrong({size.x+1,size.y},p);result=inspect(wrong,wrong,cfg);require(!result["checks"]["size"].get<bool>(),"Self-test failed: size detection");
    // Synthetic fixture: one background column, contaminated edge and isolated interior.
    const J rule{{"distance","manhattan4"},{"depth",2},{"iterations",3},{"magenta_min",30}};
    sf::Image crop({20,9},sf::Color(20,100,20));Bytes mask(180,1),removed;
    for(unsigned y=0;y<9;++y)mask[y*20]=0;
    crop.setPixel({1,4},{130,40,130});crop.setPixel({10,4},{130,40,130});
    removeFringe(crop,mask,removed,rule);
    require(!mask[4*20+1]&&removed[4*20+1],"Edge fringe must be removed");
    require(mask[4*20+10]&&mask[4*20+2],"Interior magenta and green must remain");
    const J limits{{"art_hold_checks",{"art"}}};
    require(classify(J{{"art",false},{"technical",true}},limits)=="ART_HOLD","Art-only failure classification");
    require(classify(J{{"art",false},{"technical",false}},limits)=="FAIL","Technical failure classification");
    require(classify(J{{"art",true},{"technical",true}},limits)=="PASS","Passing classification");
    if(verticalRepeat(cfg)){
        sf::Image empty(size,sf::Color::Transparent),full(size,sf::Color(10,100,10));
        require(!inspect(empty,empty,cfg)["checks"]["row0"].get<bool>(),"Empty exposed column must fail");
        require(!inspect(full,full,cfg)["checks"]["last_row"].get<bool>(),"Opaque inner column must fail");
        for(unsigned x=0;x<size.x;++x)full.setPixel({x,size.y-1},{10,10,100});
        require(!inspect(full,full,cfg)["checks"]["color_seam"].get<bool>(),"Vertical seam violation must fail");
        std::cout<<"PASS column opacity and vertical seam violation detection\n";
    }
    std::cout<<"PASS synchronous fringe edge / interior retention / data-driven classification\n";
    std::cout<<"PASS synthetic magenta / partial alpha / wrong dimensions\n";
}
// Approved-image derivation preserves source RGB; no raw segmentation or quantization.
sf::Image crevice(const sf::Image& source,const J& cfg,unsigned& face) {
    const auto size=sizeOf(cfg.at("output_size")),input=source.getSize();const auto& d=cfg.at("derivation");
    require(size.y==input.y&&size.x<=input.x,"Derived output must preserve source period");
    require(d.at("face_rule")=="after_first_max_dark_column_wrap","Unsupported face rule");
    const auto weights=d.at("luminance_weights").get<std::array<double,3>>();
    for(double w:weights)require(std::isfinite(w)&&w>=0,"Invalid luminance weight");
    const double threshold=d.at("crevice_threshold").get<double>();require(std::isfinite(threshold),"Invalid crevice threshold");
    auto dark=[&](sf::Color p){return weights[0]*p.r+weights[1]*p.g+weights[2]*p.b<threshold;};
    unsigned best=0,maximum=0;
    for(unsigned x=0;x<input.x;++x){unsigned count=0;for(unsigned y=0;y<input.y;++y)count+=dark(source.getPixel({x,y}));if(count>maximum){maximum=count;best=x;}}
    face=(best+1)%input.x;
    const unsigned full=number(d.at("full_columns"),1,size.x-1);
    const auto& hash=d.at("hash");
    const auto ym=number(hash.at("y_multiplier"),1,0xffffffffu),xm=number(hash.at("x_multiplier"),1,0xffffffffu);
    const auto mask=number(hash.at("mask"),1,0xfffffffeu),range=number(hash.at("range"),1,0xffffffffu);
    require(range==mask+1&&(range&(range-1))==0,"Hash range must match power-of-two mask");
    sf::Image image(size,sf::Color::Transparent);
    for(unsigned y=0;y<size.y;++y)for(unsigned x=0;x<size.x;++x){auto p=source.getPixel({(face+x)%input.x,y});
        const auto h=((y*ym)^((x+1)*xm))&mask;
        const double retention=std::max(0.0,1.0-(double(x)-double(full-1))/double(size.x-(full-1)));
        if(dark(p)&&(x<full||h<static_cast<unsigned>(range*retention))){p.a=255;image.setPixel({x,y},p);}
    }return image;
}
void derivedRun(const fs::path& manifest,const J& cfg,const fs::path& out,bool fixture) {
    require(verticalRepeat(cfg),"Derived crevice requires vertical orientation");protect(out,manifest,cfg);
    if(fixture){
        sf::Image image(sf::Vector2u{16u,12u});for(unsigned y=0;y<12;++y)for(unsigned x=0;x<16;++x)
            image.setPixel({x,y},((x+2*y)%5==0||x==4)?sf::Color(30,40,50):sf::Color(160,170,180));
        Outputs files;png(files,"inner.png",image);auto local=cfg;
        local["source"]={{"path","inner.png"},{"expected_size",{16,12}},{"expected_sha256",sha256(files.at("inner.png"))}};
        local["previews"]["inner"]="inner.png";local["output_size"]={12,12};local["checks"]["expected_size"]={12,12};
        json(files,"manifest.json",local);publish(out,files);return;
    }
    const auto source=resolved(manifest.parent_path()/cfg.at("source").at("path").get<std::string>());
    const auto bytes=readBytes(source);const auto hash=sha256(bytes);
    require(hash==cfg.at("source").at("expected_sha256").get<std::string>(),"Source SHA256 mismatch before image load");
    sf::Image inner;require(inner.loadFromMemory(bytes.data(),bytes.size()),"Cannot load derived source");
    require(inner.getSize()==sizeOf(cfg.at("source").at("expected_size")),"Source size mismatch");
    Outputs files;J report{{"manifest",cfg},{"manifest_path",manifest.generic_string()},{"source_sha256_before",hash},{"candidates",J::array()},
        {"not_applicable","Raw background/fringe, palette quantization, brightness, depth opacity and threshold stability checks: approved-image derivation preserves selected source RGB."}};
    require(cfg.at("candidates").size()==1,"Derived mode requires one fixed candidate");
    for(const auto& candidate:cfg.at("candidates")){
        const auto id=candidate.at("id").get<std::string>();require(!id.empty()&&id.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")==std::string::npos,"Invalid candidate id");
        unsigned face=0;const auto image=crevice(inner,cfg,face);auto stats=seams(transpose(image));
        unsigned partial=0;J colors=J::array();for(unsigned y=0;y<image.getSize().y;++y)for(unsigned x=0;x<image.getSize().x;++x){auto p=image.getPixel({x,y});partial+=p.a!=0&&p.a!=255;if(p.a&&std::find(colors.begin(),colors.end(),rgb(p))==colors.end())colors.push_back(rgb(p));}
        J checks{{"size",image.getSize()==sizeOf(cfg.at("checks").at("expected_size"))},{"binary_alpha",partial==0},
            {"color_seam",stats.at("color_seam_ratio").get<double>()<=cfg.at("checks").at("color_seam_ratio_max").get<double>()},
            {"alpha_seam",stats.at("alpha_edge_mismatches").get<double>()<=stats.at("alpha_internal_mean").get<double>()}};
        require(cfg.at("checks").at("alpha_seam")=="at_most_internal_mean","Unsupported alpha seam rule");
        stats.update({{"id",id},{"face_column",face},{"palette_rgb",colors},{"checks",checks},{"status",classify(checks,cfg.at("checks"))}});
        report["candidates"].push_back(stats);png(files,id+".png",image);
    }
    require(sha256(readBytes(source))==hash,"Source changed during run");report["source_sha256_after"]=hash;
    report["output_sha256"]=J::object();for(const auto& [name,data]:files)report["output_sha256"][name]=sha256(data);
    json(files,"report.json",report);publish(out,files);std::cout<<"DERIVED PASS: "<<out.string()<<'\n';
}

int main(int argc,char** argv) try {
    const bool fixture=argc==4&&std::string(argv[1])=="--derived-fixture";
    const bool search=argc==4&&std::string(argv[1])=="--search";
    const bool self=argc==3&&std::string(argv[1])=="--self-test";
    require(argc==((search||fixture)?4:3),"Usage: top_pipeline [--search] manifest new-output-dir | --self-test manifest");
    const auto manifest=resolved(argv[(search||self||fixture)?2:1]);const auto cfgBytes=readBytes(manifest);const J cfg=J::parse(cfgBytes);
    require(cfg.at("schema")==2&&cfg.at("kind")=="boundary_overlay","Unsupported top pipeline schema/kind");
    if(cfg.value("mode","")=="approved_inner_crevice") {require(!search&&!self,"Derived mode uses generation or fixture");derivedRun(manifest,cfg,resolved(argv[fixture?3:2]),fixture);return 0;}
    require(cfg.at("stages")==J::array({"fixed_crop","source_background_distance","optional_source_fringe","component_cleanup","nearest_color_and_mask_center","opaque_source_palette","binary_rgba"}),"Unsupported stage sequence");
    require(cfg.at("background").at("distance")=="euclidean_rgb"&&cfg.at("palette").at("method")=="stable_rgb_kmeans_row_major_v1"&&cfg.at("palette").at("sampling")=="crop_opaque_source_row_major","Unsupported background/palette method");
    require(cfg.at("checks").at("alpha_seam")=="at_most_internal_mean","Unsupported alpha seam rule");
    const bool vertical=verticalRepeat(cfg);
    require((cfg.at("previews").value("layout","floor")=="wall")==vertical,"Preview layout disagrees with orientation");
    if(self){selfTest(cfg);return 0;}
    const auto out=resolved(argv[search?3:2]);protect(out,manifest,cfg);
    const auto source=resolved(manifest.parent_path()/cfg.at("source").at("path").get<std::string>());
    const auto bytes=readBytes(source);const auto before=sha256(bytes);require(before==cfg.at("source").at("expected_sha256").get<std::string>(),"Source SHA256 mismatch before image load");
    sf::Image raw;require(raw.loadFromMemory(bytes.data(),bytes.size()),"Cannot load source");require(raw.getSize()==sizeOf(cfg.at("source").at("expected_size")),"Source size mismatch");
    const auto output=sizeOf(cfg.at("output_size"));require(output==sizeOf(cfg.at("checks").at("expected_size")),"Output size violates checks");require(output.x>1&&output.y>1,"Output too small");
    const double threshold=cfg.at("background").at("threshold").get<double>();require(threshold>0&&threshold<=std::sqrt(3*255.*255.),"Invalid threshold");
    Outputs files;J report{{"manifest",cfg},{"manifest_path",manifest.generic_string()},{"source_sha256_before",before}};
    if(search) {
        const auto& bounds=cfg.at("search");const bool explicitAxis=bounds.contains("axis");
        const unsigned from=number(bounds.at(explicitAxis?"from":"x_min")),to=number(bounds.at(explicitAxis?"to":"x_max"));require(from<=to,"Invalid search range");
        if(explicitAxis)require(bounds.at("axis")=="x"||bounds.at("axis")=="y","Invalid search axis");
        J searchCfg=cfg;if(bounds.contains("fringe"))searchCfg["fringe"]=bounds.at("fringe");
        std::vector<J> ranks;
        for(unsigned x=from;x<=to;++x){J coordinates;
            if(explicitAxis){coordinates=bounds.at("crop");coordinates[bounds.at("axis")=="x"?0:1]=x;}
            else coordinates=J::array({x,number(bounds.at("y")),number(bounds.at("width"),1),number(bounds.at("height"),1)});
            const auto c=cropOf(coordinates,raw.getSize(),output);const auto s=segment(raw,c,searchCfg,threshold);auto row=seams(vertical?transpose(s.sampled):s.sampled);
            row["crop"]=coordinates;row["alpha_pass"]=row["alpha_edge_mismatches"].get<double>()<=row["alpha_internal_mean"].get<double>();ranks.push_back(row);}
        std::stable_sort(ranks.begin(),ranks.end(),[](const J& a,const J& b){return std::tuple(!a.at("alpha_pass").get<bool>(),a.at("color_seam_ratio").get<double>(),a.at("alpha_edge_mismatches").get<unsigned>())<std::tuple(!b.at("alpha_pass").get<bool>(),b.at("color_seam_ratio").get<double>(),b.at("alpha_edge_mismatches").get<unsigned>());});
        report["ranking"]=ranks;report["ranking_rule"]=explicitAxis?
            "alpha pass first, then color ratio, then alpha mismatches; stable selected-axis ascending ties; pre-palette":
            "alpha pass first, then color ratio, then alpha mismatches; stable x ascending ties; pre-palette";
    } else {
        require(cfg.at("candidates").is_array()&&!cfg.at("candidates").empty()&&cfg.at("candidates").size()<=16,"Need one to sixteen fixed candidates");
        sf::Image inner;require(inner.loadFromFile(resolved(manifest.parent_path()/cfg.at("previews").at("inner").get<std::string>())),"Cannot load preview inner");
        unsigned partial=0;for(unsigned y=0;y<raw.getSize().y;++y)for(unsigned x=0;x<raw.getSize().x;++x){auto a=raw.getPixel({x,y}).a;partial+=a!=0&&a!=255;}
        report["source_partial_alpha"]=partial;report["source_png_color_type"]=bytes.at(25);report["source_has_alpha_channel"]=bytes.at(25)==4||bytes.at(25)==6;
        report["candidates"]=J::array();std::vector<sf::Image> sheets,images;
        for(const auto& candidate:cfg.at("candidates")) {
            const auto id=candidate.at("id").get<std::string>();require(!id.empty()&&id.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")==std::string::npos,"Invalid candidate id");
            const auto c=cropOf(candidate.at("crop"),raw.getSize(),output);J local=cfg;
            if(candidate.contains("threshold"))local["background"]["threshold"]=candidate.at("threshold");
            if(candidate.contains("fringe"))local["fringe"]=candidate.at("fringe");
            const double t=local.at("background").at("threshold").get<double>();require(t>0&&t<=std::sqrt(3*255.*255.),"Invalid candidate threshold");
            const auto s=segment(raw,c,local,t);const auto colors=palette(s,local);auto image=quantize(s.sampled,colors);auto stats=inspect(image,s.sampled,local);stats.update(s.stats);
            stats["id"]=id;stats["crop"]=candidate.at("crop");stats["threshold"]=t;stats["reason"]=candidate.at("reason");stats["mask_cleanup"]=cfg.at("mask_cleanup");
            stats["observations_in_crop"]=J::array();for(const auto& observation:cfg.at("observations")){const auto point=observation.at("point").get<std::array<unsigned,2>>();if(point[0]>=c.x&&point[0]<c.x+c.w&&point[1]>=c.y&&point[1]<c.y+c.h)stats["observations_in_crop"].push_back(observation);}
            double maxDifference=0;J changes=J::array();const double delta=fraction(cfg.at("checks").at("stability_delta"));
            for(double multiplier:{1-delta,1+delta}) {const auto changed=segment(raw,c,local,t*multiplier);unsigned different=0;
                for(unsigned y=0;y<output.y;++y)for(unsigned x=0;x<output.x;++x)different+=s.sampled.getPixel({x,y}).a!=changed.sampled.getPixel({x,y}).a;
                const double f=double(different)/(output.x*output.y);changes.push_back(f);maxDifference=std::max(maxDifference,f);}
            stats["stability_alpha_changes"]=changes;stats["checks"]["stability"]=maxDifference<=fraction(cfg.at("checks").at("stability_change_max"));
            if(local.contains("fringe")){
                J sensitivity=J::array();const auto& rule=local.at("fringe");
                for(const auto& [field,deltaValue]:rule.at("sensitivity_deltas").items()){
                    require(field=="depth"||field=="magenta_min"||field=="iterations","Unknown fringe sensitivity field");
                    const double amount=deltaValue.get<double>();require(std::isfinite(amount)&&amount>0,"Invalid sensitivity delta");
                    for(int sign:{-1,1}){J adjusted=local;const double value=rule.at(field).get<double>()+sign*amount;
                        if(field=="magenta_min")adjusted["fringe"][field]=value;
                        else {require(value>=1&&value<=32&&std::floor(value)==value,"Invalid sensitivity integer");adjusted["fringe"][field]=static_cast<unsigned>(value);}
                        const auto changed=segment(raw,c,adjusted,t);
                        sensitivity.push_back({{"field",field},{"value",value},{"alpha_fraction",alphaChanges(s.sampled,changed.sampled).at("fraction")}});
                    }
                }stats["fringe"]["sensitivity"]=sensitivity;
                auto marked=s.crop;for(unsigned y=0;y<c.h;++y)for(unsigned x=0;x<c.w;++x)if(s.removed[y*c.w+x])marked.setPixel({x,y},color(cfg.at("previews").at("removal_rgb")));
                png(files,id+"_removed_source.png",marked);png(files,id+"_removed_source_zoom.png",scale(marked,number(cfg.at("previews").at("inspect_scale"),1,8)));
            }
            stats["status"]=classify(stats["checks"],cfg.at("checks"));
            J differences=J::array();for(const auto& other:images){unsigned count=0;for(unsigned y=0;y<output.y;++y)for(unsigned x=0;x<output.x;++x)count+=image.getPixel({x,y})!=other.getPixel({x,y});differences.push_back(double(count)/(output.x*output.y));}
            stats["different_pixel_fraction_previous_candidates"]=differences;images.push_back(image);
            png(files,id+".png",image);sheets.push_back(previews(files,id,s,image,inner,cfg.at("previews")));report["candidates"].push_back(stats);
            std::cout<<id<<": "<<stats.at("status")<<" seam="<<stats.at("color_seam_ratio")<<" alpha="<<stats.at("alpha_edge_mismatches")<<" bright="<<stats.at("bright_fraction")<<'\n';
        }
        const auto size=sheets.front().getSize();sf::Image sheet(sf::Vector2u{size.x,size.y*static_cast<unsigned>(sheets.size())});
        for(unsigned i=0;i<sheets.size();++i)require(sheet.copy(sheets[i],{0,i*size.y}),"Sheet copy failed");
        png(files,"comparison.png",sheet);png(files,"comparison_game.png",scale(sheet,number(cfg.at("previews").at("game_scale"),1,8)));
        report["comparisons"]=J::array();
        for(const auto& pair:cfg.value("comparisons",J::array())){
            const auto first=pair.at("before").get<std::string>(),last=pair.at("after").get<std::string>();
            const auto lookup=[&](const std::string& id)->const J&{for(const auto& row:report.at("candidates"))if(row.at("id")==id)return row;throw std::runtime_error("Unknown comparison candidate");};
            const auto& a=lookup(first);const auto& b=lookup(last);require(a.at("crop")==b.at("crop"),"Comparison crops differ");
            const auto prefix=first+"_to_"+last;
            for(const auto& suffix:{"_mask_zoom.png","_checker_zoom.png","_world_game.png"})
                png(files,prefix+suffix,stacked(decoded(files,first+suffix),decoded(files,last+suffix)));
            const auto beforeImage=decoded(files,first+".png"),afterImage=decoded(files,last+".png");
            auto marked=afterImage;const auto change=alphaChanges(beforeImage,afterImage);
            for(unsigned y=0;y<output.y;++y)for(unsigned x=0;x<output.x;++x)if(beforeImage.getPixel({x,y}).a!=afterImage.getPixel({x,y}).a)marked.setPixel({x,y},color(cfg.at("previews").at("removal_rgb")));
            png(files,prefix+"_alpha_changes.png",marked);png(files,prefix+"_alpha_changes_zoom.png",scale(marked,number(cfg.at("previews").at("inspect_scale"),1,8)));
            J comparison{{"before",first},{"after",last},{"output_alpha_changes",change}};
            for(const auto& field:{"row_opacity","palette_rgb","magenta_palette_colors","magenta_output_pixels"})comparison[field]={{"before",a.at(field)},{"after",b.at(field)}};
            report["comparisons"].push_back(comparison);
        }
    }
    const auto after=sha256(readBytes(source));require(after==before,"Source changed during run");report["source_sha256_after"]=after;
    report["output_sha256"]=J::object();for(const auto& [name,data]:files)report["output_sha256"][name]=sha256(data);
    json(files,search?"ranking.json":"report.json",report);publish(out,files);std::cout<<"OK: "<<files.size()<<" files; source SHA256="<<after<<'\n';return 0;
} catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
