#pragma once
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
namespace PipelineFiles {
namespace fs = std::filesystem;
inline fs::path resolved(const fs::path& path) { return fs::weakly_canonical(fs::absolute(path)); }
inline std::string pathKey(const fs::path& path)
{
    auto key=resolved(path).generic_string();
#ifdef _WIN32
    std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
#endif
    return key;
}
inline bool within(const fs::path& child,const fs::path& parent)
{
    auto a=pathKey(child),b=pathKey(parent);
    if(a==b) return true;
    if(b.back()!='/') b+='/';
    return a.starts_with(b);
}
inline std::vector<std::uint8_t> readBytes(const fs::path& path)
{
    std::ifstream file(path,std::ios::binary);
    if(!file) throw std::runtime_error("Cannot read: "+path.string());
    return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
// Exclusive creation also protects against a file appearing after preflight.
inline void writeNew(const fs::path& path,const std::vector<std::uint8_t>& bytes)
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
}
