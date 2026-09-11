#include "Background.hpp"
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <nlohmann/json.hpp>
#include <cmath>
#include <fstream>
#include <stdexcept>

std::vector<BackgroundLayer> Background::readLayers(const std::filesystem::path& file) try
{
    if (!std::filesystem::exists(file)) return {}; // Region without a background keeps its flat color.
    std::ifstream input(file);
    if (!input) throw std::runtime_error("Cannot open file");
    const auto data = nlohmann::json::parse(input);
    const auto& entries = data.at("layers");
    if (!entries.is_array()) throw std::runtime_error("layers must be an array");
    const auto vector = [](const nlohmann::json& value) {
        if (!value.is_array() || value.size() != 2 || !value[0].is_number() || !value[1].is_number())
            throw std::runtime_error("Expected two numeric coordinates");
        const sf::Vector2f result{value[0].get<float>(), value[1].get<float>()};
        if (!std::isfinite(result.x) || !std::isfinite(result.y)) throw std::runtime_error("Nonfinite coordinate");
        return result;
    };
    std::vector<BackgroundLayer> result;
    for (const auto& entry : entries) {
        const std::filesystem::path image(entry.at("image").get<std::string>());
        if (image.empty() || image.is_absolute()) throw std::runtime_error("Expected relative image path");
        const auto p = vector(entry.at("parallax"));
        if (p.x < 0 || p.x > 1 || p.y < 0 || p.y > 1) throw std::runtime_error("Parallax outside 0..1");
        result.push_back({file.parent_path()/image, p, vector(entry.at("anchor")), entry.at("repeatX").get<bool>()});
    }
    return result;
}
catch (const std::exception& error) {
    throw std::runtime_error("Background " + file.string() + ": " + error.what());
}

Background::Background(const std::filesystem::path& file)
{
    for (auto data : readLayers(file)) {
        sf::Texture texture;
        if (!texture.loadFromFile(data.image)) throw std::runtime_error("Cannot load background image: " + data.image.string());
        texture.setSmooth(false);
        texture.setRepeated(data.repeatX);
        layers_.push_back({std::move(data), std::move(texture)});
    }
}

sf::Vector2f Background::position(const BackgroundLayer& layer, sf::Vector2f center)
{
    return layer.anchor + sf::Vector2f{center.x*(1.f-layer.parallax.x), center.y*(1.f-layer.parallax.y)};
}

std::optional<sf::FloatRect> Background::visibleRect(const BackgroundLayer& layer,
                                                   sf::Vector2u imageSize, const sf::View& view)
{
    const sf::FloatRect screen{view.getCenter()-view.getSize()/2.f, view.getSize()};
    auto origin = position(layer, view.getCenter());
    sf::Vector2f size(imageSize);
    if (layer.repeatX) { origin.x = screen.position.x; size.x = screen.size.x; }
    return screen.findIntersection({origin,size});
}

sf::VertexArray Background::makeQuad(const sf::FloatRect& visible, sf::Vector2f origin)
{
    const auto a = visible.position;
    const auto b = a + visible.size;
    sf::VertexArray quad(sf::PrimitiveType::Triangles, 6);
    const sf::Vector2f corners[]{a,{b.x,a.y},b,a,b,{a.x,b.y}};
    for (std::size_t i=0; i<6; ++i)
        quad[i] = sf::Vertex{corners[i],sf::Color::White,corners[i]-origin};
    return quad;
}

void Background::render(sf::RenderTarget& target) const
{
    const auto& view = target.getView();
    for (const auto& layer : layers_) {
        const auto visible = visibleRect(layer.data, layer.texture.getSize(), view);
        if (!visible) continue;
        const auto quad = makeQuad(*visible, position(layer.data,view.getCenter()));
        sf::RenderStates states;
        states.texture = &layer.texture;
        target.draw(quad,states);
    }
}
