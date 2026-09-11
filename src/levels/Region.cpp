#include "Region.hpp"
#include "../Display.hpp"
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <nlohmann/json.hpp>
#include <cmath>
#include <fstream>
#include <stdexcept>

std::filesystem::path Region::findFile(const std::string& name, std::filesystem::path directory)
{
    if (name.empty() || name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") != std::string::npos)
        throw std::runtime_error("Invalid region name: " + name);
    directory = std::filesystem::absolute(directory);
    while (true)
    {
        const auto candidate = directory / "assets" / "regions" / (name + ".json");
        if (std::filesystem::is_regular_file(candidate)) return candidate;
        const auto parent = directory.parent_path();
        if (parent == directory) break;
        directory = parent;
    }
    throw std::runtime_error("Region JSON not found: " + name);
}

Region::Region(const std::filesystem::path& file) try
{
    std::ifstream input(file);
    if (!input) throw std::runtime_error("Cannot open file");
    const auto data = nlohmann::json::parse(input);
    if (data.at("id").get<std::string>().empty()) throw std::runtime_error("Empty region id");
    const auto numbers = [](const nlohmann::json& value, std::size_t count) {
        if (!value.is_array() || value.size() != count) throw std::runtime_error("Invalid coordinate array");
        std::vector<float> result;
        for (const auto& item : value) {
            if (!item.is_number()) throw std::runtime_error("Coordinate must be numeric");
            const float number = item.get<float>();
            if (!std::isfinite(number)) throw std::runtime_error("Coordinate must be finite");
            result.push_back(number);
        }
        return result;
    };
    const auto b = numbers(data.at("bounds"), 4);
    bounds_ = {{b[0], b[1]}, {b[2], b[3]}};
    // Current Player clamps X to 0..roomWidth; current reference camera needs this minimum extent.
    if (b[0] != 0 || b[2] < Display::referenceViewSize.x || b[3] < Display::referenceViewSize.y) throw std::runtime_error("Unsupported region bounds");
    const auto s = numbers(data.at("spawn"), 2);
    spawn_ = {s[0], s[1]};
    if (!bounds_.contains(spawn_)) throw std::runtime_error("Spawn outside region");
    const auto& geometry = data.at("solids");
    if (!geometry.is_array() || geometry.empty()) throw std::runtime_error("Solids must be a nonempty array");
    for (const auto& value : geometry) {
        const auto r = numbers(value, 4);
        if (r[2] <= 0 || r[3] <= 0 || r[0] < b[0] || r[1] < b[1] ||
            r[0]+r[2] > b[0]+b[2] || r[1]+r[3] > b[1]+b[3])
            throw std::runtime_error("Solid has invalid size or exceeds region");
        solids_.push_back({{r[0],r[1]}, {r[2],r[3]}});
    }
}
catch (const std::exception& error) {
    throw std::runtime_error("Region " + file.string() + ": " + error.what());
}

void Region::render(sf::RenderTarget& target) const
{
    sf::RectangleShape terrain;
    terrain.setFillColor(sf::Color(70,80,90));
    for (const auto& solid : solids_) {
        terrain.setPosition(solid.position);
        terrain.setSize(solid.size);
        target.draw(terrain);
    }
}
