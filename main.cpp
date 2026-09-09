#include <SFML/Graphics.hpp>

#include <algorithm>
#include <optional>

namespace
{
constexpr unsigned int windowWidth = 800;
constexpr unsigned int windowHeight = 600;
constexpr float floorY = 520.f;
constexpr float playerWidth = 32.f;
constexpr float playerHeight = 48.f;
constexpr float moveSpeed = 260.f;
constexpr float jumpSpeed = 520.f;
constexpr float gravity = 1500.f;

struct InputIntent
{
    float direction = 0.f;
    bool jumpRequested = false;
};

struct Player
{
    sf::Vector2f position{100.f, floorY - playerHeight};
    sf::Vector2f velocity{0.f, 0.f};
    bool grounded = true;
};

InputIntent readInput(bool focused, bool jumpRequested)
{
    if (!focused)
        return {};

    const bool left = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) ||
                      sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left);
    const bool right = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) ||
                       sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right);
    return {static_cast<float>(right) - static_cast<float>(left), jumpRequested};
}

void updatePlayer(Player& player, const InputIntent& intent, float deltaTime)
{
    player.velocity.x = intent.direction * moveSpeed;
    if (intent.jumpRequested && player.grounded)
    {
        player.velocity.y = -jumpSpeed;
        player.grounded = false;
    }

    player.velocity.y += gravity * deltaTime;
    player.position += player.velocity * deltaTime;

    player.position.x = std::clamp(player.position.x, 0.f,
                                   static_cast<float>(windowWidth) - playerWidth);

    player.grounded = player.position.y + playerHeight >= floorY;
    if (player.grounded)
    {
        player.position.y = floorY - playerHeight;
        player.velocity.y = 0.f;
    }
}
} // namespace

int main()
{
    sf::RenderWindow window(sf::VideoMode({windowWidth, windowHeight}),
                            "Project Slit Prototype",
                            sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);

    sf::RectangleShape floor({static_cast<float>(windowWidth),
                              static_cast<float>(windowHeight) - floorY});
    floor.setPosition({0.f, floorY});
    floor.setFillColor(sf::Color(70, 80, 90));

    Player player;
    sf::RectangleShape playerShape({playerWidth, playerHeight});
    playerShape.setFillColor(sf::Color(240, 200, 100));

    sf::Clock clock;
    while (window.isOpen())
    {
        // Discard long stalls so dragging the window cannot cause a large step.
        const float deltaTime = std::min(clock.restart().asSeconds(), 0.05f);
        bool jumpRequested = false;
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();
            if (const auto* key = event->getIf<sf::Event::KeyPressed>())
                jumpRequested |= key->code == sf::Keyboard::Key::Space;
        }

        if (!window.isOpen())
            break;

        const InputIntent intent = readInput(window.hasFocus(), jumpRequested);
        updatePlayer(player, intent, deltaTime);
        playerShape.setPosition(player.position);

        window.clear(sf::Color(25, 30, 45));
        window.draw(floor);
        window.draw(playerShape);
        window.display();
    }
}
