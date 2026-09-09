#include "Camera.hpp"

#include <algorithm>
#include <cmath>

void Camera::follow(sf::View& view, sf::Vector2f playerCenter, const sf::FloatRect& world, float deltaTime)
{
    const auto halfView = view.getSize() / 2.f;
    const float halfDeadZone = verticalDeadZone / 2.f;
    const float oldY = view.getCenter().y;
    float targetY = oldY;
    if (playerCenter.y < oldY - halfDeadZone)
        targetY = playerCenter.y + halfDeadZone;
    else if (playerCenter.y > oldY + halfDeadZone)
        targetY = playerCenter.y - halfDeadZone;

    const float minY = world.position.y + halfView.y;
    const float maxY = world.position.y + world.size.y - halfView.y;
    targetY = std::clamp(targetY, minY, maxY);
    const float blend = 1.f - std::exp(-cameraFollowSpeed * deltaTime);
    view.setCenter({std::clamp(playerCenter.x, world.position.x + halfView.x,
                              world.position.x + world.size.x - halfView.x),
                    std::clamp(oldY + (targetY - oldY) * blend, minY, maxY)});
}
