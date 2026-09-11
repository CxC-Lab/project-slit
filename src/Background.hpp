#pragma once
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/View.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <filesystem>
#include <optional>
#include <vector>
namespace sf { class RenderTarget; }

struct BackgroundLayer
{
    std::filesystem::path image;
    sf::Vector2f parallax;
    sf::Vector2f anchor;
    bool repeatX;
};

class Background
{
public:
    explicit Background(const std::filesystem::path& file);
    static std::vector<BackgroundLayer> readLayers(const std::filesystem::path& file);
    // Intersect the layer with the view before submitting a single textured quad.
    static std::optional<sf::FloatRect> visibleRect(const BackgroundLayer& layer,
                                                   sf::Vector2u imageSize, const sf::View& view);
    static sf::Vector2f position(const BackgroundLayer& layer, sf::Vector2f cameraCenter);
    static sf::VertexArray makeQuad(const sf::FloatRect& visible, sf::Vector2f origin);
    void render(sf::RenderTarget& target) const;
private:
    struct Layer { BackgroundLayer data; sf::Texture texture; };
    std::vector<Layer> layers_;
};
