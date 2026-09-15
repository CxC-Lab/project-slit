#pragma once
#include <SFML/Graphics.hpp>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <vector>
#include <optional>

struct DecorationSelection { std::optional<std::filesystem::path> file; const char* mode; };
DecorationSelection selectDecorations(const std::optional<std::filesystem::path>& regionDefault,
    const std::string& option, bool tilesetOverride);

class Decorations
{
public:
    enum class Layer { Behind, Above };
    explicit Decorations(const std::filesystem::path& file);
    // Returns submitted item count; offscreen items produce no vertices.
    std::size_t render(sf::RenderTarget& target, Layer layer) const;
    nlohmann::json captureIdentity() const;
private:
    struct Item { std::size_t texture; sf::Vector2f position; Layer layer; bool flip; };
    std::filesystem::path file_;
    std::string hash_;
    std::vector<sf::Texture> textures_;
    std::vector<Item> items_;
};
