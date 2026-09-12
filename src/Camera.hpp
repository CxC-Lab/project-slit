#pragma once

#include <SFML/Graphics/View.hpp>

namespace Camera
{
// Playtest candidates: full vertical dead-zone height and exponential follow rate.
inline constexpr float verticalDeadZone = 240.f;
inline constexpr float cameraFollowSpeed = 6.f;
inline constexpr float catchUpMultiplier = 2.5f;
inline constexpr float catchUpStartSpeed = 520.f;
inline constexpr float catchUpFullSpeed = 1000.f;
inline constexpr float ascentExtra = 0.03f;
float speedMultiplier(float verticalVelocity);
void follow(sf::View& view, sf::Vector2f playerCenter, const sf::FloatRect& world, float deltaTime, float followMultiplier = 1.f);
}
