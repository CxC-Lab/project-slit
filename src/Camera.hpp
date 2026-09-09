#pragma once

#include <SFML/Graphics/View.hpp>

namespace Camera
{
// Playtest candidates: full vertical dead-zone height and exponential follow rate.
inline constexpr float verticalDeadZone = 240.f;
inline constexpr float cameraFollowSpeed = 6.f;
void follow(sf::View& view, sf::Vector2f playerCenter, const sf::FloatRect& world, float deltaTime);
}
