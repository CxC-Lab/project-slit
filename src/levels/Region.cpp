#include "Region.hpp"
#include "../Display.hpp"
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <nlohmann/json.hpp>
#include <cmath>
#include <algorithm>
#include <SFML/Graphics/VertexArray.hpp>
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
    const auto& view = target.getView();
    const auto visible = bounds_.findIntersection({view.getCenter() - view.getSize()/2.f, view.getSize()});
    if (!visible) return;
    const auto lo = visible->position;
    const auto hi = lo + visible->size;
    sf::VertexArray guides(sf::PrimitiveType::Lines);
    const auto line = [&](sf::Vector2f a, sf::Vector2f b, sf::Color color) {
        guides.append(sf::Vertex{a, color});
        guides.append(sf::Vertex{b, color});
    };
    // One major cell is the established 16:9 composition; four subdivisions per axis.
    constexpr sf::Vector2f cell{800.f, 450.f};
    constexpr sf::Color minor(30,36,51), major(39,46,61), marker(43,55,66);
    const auto grid = [&](bool vertical) {
        const float origin = vertical ? bounds_.position.x : bounds_.position.y;
        const float start = vertical ? lo.x : lo.y;
        const float end = vertical ? hi.x : hi.y;
        const float step = (vertical ? cell.x : cell.y) / 4.f;
        for (int i = static_cast<int>(std::ceil((start-origin)/step)); origin+i*step <= end; ++i) {
            const float at = origin+i*step;
            line(vertical ? sf::Vector2f{at,lo.y} : sf::Vector2f{lo.x,at},
                 vertical ? sf::Vector2f{at,hi.y} : sf::Vector2f{hi.x,at}, i%4 == 0 ? major : minor);
        }
    };
    grid(true);
    grid(false);
    // Hollow diamond beacons have no horizontal ledge or solid fill: never terrain.
    // Bound-relative quarter points work in both regions, without extra JSON geometry.
    for (int column = 1; column <= 3; ++column) {
        const sf::Vector2f center = bounds_.position + sf::Vector2f{
            bounds_.size.x * column / 4.f, bounds_.size.y / 3.f};
        const sf::Vector2f radius{80.f, 150.f + 50.f*column};
        if (!visible->findIntersection({center-radius, radius*2.f})) continue;
        line(center-sf::Vector2f{0,radius.y}, center+sf::Vector2f{radius.x,0}, marker);
        line(center+sf::Vector2f{radius.x,0}, center+sf::Vector2f{0,radius.y}, marker);
        line(center+sf::Vector2f{0,radius.y}, center-sf::Vector2f{radius.x,0}, marker);
        line(center-sf::Vector2f{radius.x,0}, center-sf::Vector2f{0,radius.y}, marker);
        for (int i=0; i<column; ++i) {
            const float y = center.y + (i-(column-1)/2.f)*24.f;
            line({center.x-10.f,y-8.f},{center.x+10.f,y+8.f},marker);
            line({center.x-10.f,y+8.f},{center.x+10.f,y-8.f},marker);
        }
    }
    target.draw(guides);
    guides.clear();
    sf::RectangleShape terrain;
    terrain.setFillColor(sf::Color(70,80,90));
    for (const auto& solid : solids_) {
        const auto part = solid.findIntersection(*visible);
        if (!part) continue;
        terrain.setPosition(solid.position);
        terrain.setSize(solid.size);
        target.draw(terrain);
        const auto end = solid.position + solid.size;
        // Inset notches stay within filled terrain, rather than suggesting new platforms.
        for (float x = bounds_.position.x + std::ceil((part->position.x-bounds_.position.x)/100.f)*100.f;
             x <= part->position.x+part->size.x; x += 100.f) {
            if (solid.position.y >= lo.y && solid.position.y <= hi.y)
                line({x,solid.position.y},{x,solid.position.y+std::min(8.f,solid.size.y)},minor);
        }
        for (float y = bounds_.position.y + std::ceil((part->position.y-bounds_.position.y)/75.f)*75.f;
             y <= part->position.y+part->size.y; y += 75.f) {
            for (const float x : {solid.position.x,end.x})
                if (x >= lo.x && x <= hi.x)
                    line({x,y},{x+(x == end.x ? -1.f : 1.f)*std::min(8.f,solid.size.x),y},minor);
        }
    }
    target.draw(guides);
}
