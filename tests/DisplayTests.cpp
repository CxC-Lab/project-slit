#include "Display.hpp"
#include "IdleAnimation.hpp"

#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

float drawAndMeasure(sf::RenderWindow& window, const sf::View& view, sf::Sprite& sprite)
{
    window.setView(view);
    window.clear(sf::Color::Black);
    const sf::Color backgroundColor(25, 30, 45);
    sf::RectangleShape background(view.getSize());
    background.setPosition(view.getCenter() - view.getSize() / 2.f);
    background.setFillColor(backgroundColor);
    window.draw(background);
    sf::Texture capture(window.getSize());
    capture.update(window);
    const auto backgroundImage = capture.copyToImage();
    sprite.setPosition(view.getCenter());
    window.draw(sprite);
    capture.update(window);
    const auto image = capture.copyToImage();
    std::size_t pixels = 0;
    for (unsigned int y = 0; y < image.getSize().y; ++y)
        for (unsigned int x = 0; x < image.getSize().x; ++x)
        {
            const auto color = image.getPixel({x, y});
            check(backgroundImage.getPixel({x, y}) == backgroundColor,
                  "Cover background fills every physical pixel without black bars");
            pixels += color != backgroundColor;
        }
    window.display();
    check(pixels > 0, "sprite remains visible after window recreation");
    const float scale = window.getSize().y * view.getViewport().size.y / view.getSize().y;
    return static_cast<float>(pixels) / (scale * scale);
}

void gui()
{
    Display display;
    sf::RenderWindow window;
    display.open(window);
    sf::View view(sf::FloatRect({0.f, 0.f}, Display::referenceViewSize));
    view.setCenter({1000.f, -300.f});
    Display::apply(view, window.getSize());
    const auto center = view.getCenter();
    const auto windowedSize = window.getSize();
    const auto windowedPosition = window.getPosition();
    IdleAnimation idle(IdleAnimation::findManifest());
    sf::Texture texture(idle.atlasPath());
    idle.validateAtlas(texture.getSize());
    texture.setSmooth(false);
    const auto original = texture.copyToImage();
    sf::Sprite sprite(texture, idle.frameRect());
    sprite.setOrigin(idle.pivot());
    sprite.setScale({0.5f, 0.5f});
    const float originalArea = drawAndMeasure(window, view, sprite);
    const sf::Event pressed = sf::Event::KeyPressed{.code = sf::Keyboard::Key::Enter, .alt = true};
    const sf::Event released = sf::Event::KeyReleased{.code = sf::Keyboard::Key::Enter};
    for (int i = 0; i < 2; ++i)
    {
        check(display.handleEvent(pressed, window, view), "Alt Enter consumed by display");
        check(display.fullscreen() == (i == 0), "mode toggles");
        check(view.getCenter() == center, "toggle preserves camera center");
        display.handleEvent(released, window, view);
        if (i == 0)
        {
            check(window.getSize() == sf::VideoMode::getDesktopMode().size, "native desktop resolution");
            check(window.getPosition() == sf::Vector2i(0, 0), "desktop origin");
#ifdef _WIN32
            const auto style = GetWindowLongPtr(window.getNativeHandle(), GWL_STYLE);
            check((style & (WS_CAPTION | WS_THICKFRAME)) == 0, "native borderless style");
#endif
        }
        else
        {
            check(window.getSize() == windowedSize, "windowed client size restored");
            check(window.getPosition() == windowedPosition, "windowed position restored");
        }
        check(!texture.isSmooth() && sprite.getScale() == sf::Vector2f(0.5f, 0.5f), "filter and visual scale unchanged");
        const auto restored = texture.copyToImage();
        check(restored.getSize() == original.getSize(), "texture dimensions survive recreation");
        check(std::equal(original.getPixelsPtr(), original.getPixelsPtr() + original.getSize().x * original.getSize().y * 4,
                         restored.getPixelsPtr()), "actual Idle RGBA pixels survive recreation");
        const float area = drawAndMeasure(window, view, sprite);
        check(std::abs(area / originalArea - 1.f) < 0.15f, "uniformly scaled visible silhouette area");
        std::cout << "GUI mode=" << (display.fullscreen() ? "borderless" : "windowed")
                  << " physical=" << window.getSize().x << 'x' << window.getSize().y
                  << " logical=" << view.getSize().x << 'x' << view.getSize().y
                  << " full coverage=OK normalized silhouette area=" << area << " texture=OK\n";
    }
#ifdef _WIN32
    // Native close request uses the same WM_CLOSE path as Alt+F4.
    PostMessage(window.getNativeHandle(), WM_CLOSE, 0, 0);
    bool closedEvent = false;
    for (int i = 0; i < 30 && !closedEvent; ++i)
    {
        while (const auto event = window.pollEvent())
            closedEvent |= event->is<sf::Event::Closed>();
        sf::sleep(sf::milliseconds(10));
    }
    check(closedEvent, "native close event preserved");
#endif
    window.close();
}

int main(int argc, char** argv) try
{
    for (const sf::Vector2u physical : {sf::Vector2u{800, 600}, {1920, 1080}, {3840, 2160}, {2520, 1080},
                                       {1080, 1920}, {10000, 100}, {100, 10000}, {1, 1}})
    {
        sf::View view;
        view.setCenter({900.f, -300.f});
        Display::apply(view, physical);
        const auto viewport = view.getViewport();
        check(view.getSize().x <= Display::referenceViewSize.x + 0.0001f &&
              view.getSize().y <= Display::referenceViewSize.y + 0.0001f, "Cover only crops the reference view");
        check(std::abs(view.getSize().x - Display::referenceViewSize.x) < 0.0001f ||
              std::abs(view.getSize().y - Display::referenceViewSize.y) < 0.0001f, "Cover preserves one reference axis");
        check(view.getCenter() == sf::Vector2f(900.f, -300.f), "resize preserves camera center");
        const float sx = physical.x * viewport.size.x / view.getSize().x;
        const float sy = physical.y * viewport.size.y / view.getSize().y;
        check(std::abs(sx - sy) < 0.0001f, "equal XY scaling without aspect distortion");
        check(viewport == sf::FloatRect({0.f, 0.f}, {1.f, 1.f}), "viewport covers entire screen");
        check(view.getSize().x <= 3200.f && view.getSize().y <= 1800.f, "practice camera clamp interval remains valid");
    }
    sf::View wide;
    Display::apply(wide, {1920, 1080});
    check(std::abs(wide.getSize().x - 800.f) < 0.0001f &&
          std::abs(wide.getSize().y - 450.f) < 0.0001f, "16:9 crops vertical world to 450");
    for (const sf::Vector2u minimized : {sf::Vector2u{0, 0}, {0, 600}, {800, 0}})
    {
        Display::apply(wide, minimized);
        check(wide.getSize() == Display::referenceViewSize, "minimized view stays finite");
    }
    check(Display::viewport({0, 0}).size == sf::Vector2f(1.f, 1.f), "zero resize avoids division by zero");
    if (argc > 1 && std::string(argv[1]) == "--gui") gui();
    std::cout << "PASS: display\n";
}
catch (const std::exception& error)
{
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
}
