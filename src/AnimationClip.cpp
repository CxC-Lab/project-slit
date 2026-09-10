#include "AnimationClip.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{
int integer(const nlohmann::json& value, int minimum)
{
    if (!value.is_number_integer() || value.get<double>() < minimum ||
        value.get<double>() > std::numeric_limits<int>::max())
        throw std::runtime_error("invalid canvas/frame/row integer");
    return value.get<int>();
}
}

std::filesystem::path AnimationClip::findManifest(std::filesystem::path directory)
{
    directory = std::filesystem::absolute(directory);
    for (;;)
    {
        const auto candidate = directory / "assets/characters/proto_one/proto_one.manifest.json";
        if (std::filesystem::is_regular_file(candidate))
            return candidate;
        const auto parent = directory.parent_path();
        if (parent == directory) break;
        directory = parent;
    }
    throw std::runtime_error("proto_one manifest not found in working directory or its parents");
}

AnimationClip::AnimationClip(const std::filesystem::path& manifest, const std::string& clipName)
{
    try
    {
        std::ifstream input(manifest);
        if (!input) throw std::runtime_error("cannot open manifest");
        const auto data = nlohmann::json::parse(input);
        const auto& canvas = data.at("canvas");
        if (!canvas.is_array() || canvas.size() != 2)
            throw std::runtime_error("canvas must contain exactly two integers");
        canvas_ = {integer(canvas.at(0), 1), integer(canvas.at(1), 1)};
        pivot_ = {data.at("pivot").at("x").get<float>(), data.at("pivot").at("y").get<float>()};
        if (!std::isfinite(pivot_.x) || !std::isfinite(pivot_.y) || pivot_.x < 0.f || pivot_.y < 0.f ||
            pivot_.x > canvas_.x || pivot_.y > canvas_.y)
            throw std::runtime_error("pivot must be finite and inside the canvas");
        const auto& clip = data.at("animations").at(clipName);
        row_ = integer(clip.at("row"), 0);
        frames_ = integer(clip.at("frames"), 1);
        fps_ = clip.at("fps").get<double>();
        loop_ = clip.at("loop").get<bool>();
        if (!std::isfinite(fps_) || fps_ <= 0. || !std::isfinite(frames_ / fps_))
            throw std::runtime_error("Animation fps must be finite and positive");
        if (static_cast<long long>(frames_) * canvas_.x > std::numeric_limits<int>::max() ||
            (static_cast<long long>(row_) + 1) * canvas_.y > std::numeric_limits<int>::max())
            throw std::runtime_error("Animation frame rectangles exceed integer range");
        const auto atlasKey = clip.at("atlas").get<std::string>();
        const std::filesystem::path relative(data.at("atlas").at(atlasKey).get<std::string>());
        if (relative.is_absolute() || relative.parent_path() != "atlas" || relative.filename().empty())
            throw std::runtime_error("Animation atlas must be a file directly under atlas/");
        atlasPath_ = manifest.parent_path() / relative;
        if (!std::filesystem::is_regular_file(atlasPath_))
            throw std::runtime_error("Animation atlas file does not exist: " + atlasPath_.string());
    }
    catch (const std::exception& error)
    {
        throw std::runtime_error("Animation " + clipName + " manifest " + manifest.string() + ": " + error.what());
    }
}

void AnimationClip::validateAtlas(sf::Vector2u size) const
{
    if (size.x != static_cast<unsigned int>(frames_ * canvas_.x) ||
        size.y < static_cast<unsigned int>((static_cast<long long>(row_) + 1) * canvas_.y) ||
        size.y % static_cast<unsigned int>(canvas_.y) != 0)
        throw std::runtime_error("Animation atlas dimensions do not match canvas, frame count and row: " + atlasPath_.string());
}

void AnimationClip::advance(double deltaTime)
{
    if (!std::isfinite(deltaTime) || deltaTime < 0.)
        throw std::runtime_error("Animation delta time must be finite and nonnegative");
    const double duration = frames_ / fps_;
    if (loop_)
        elapsed_ = std::fmod(elapsed_ + std::fmod(deltaTime, duration), duration);
    else
        elapsed_ = std::min(elapsed_ + deltaTime, duration);
}

int AnimationClip::frameIndex() const
{
    return static_cast<int>(std::min(std::floor(elapsed_ * fps_), static_cast<double>(frames_ - 1)));
}

sf::IntRect AnimationClip::frameRect() const
{
    return {{frameIndex() * canvas_.x, row_ * canvas_.y}, canvas_};
}
