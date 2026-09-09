#include "Player.hpp"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float contactTolerance = 0.001f;
constexpr float maxStep = 1.f / 120.f;
bool overlaps(float a, float sizeA, float b, float sizeB)
{
    return a < b + sizeB && a + sizeA > b;
}
}

Player::Player(sf::Vector2f position) : position_(position) {}

sf::FloatRect Player::collisionBounds() const
{
    return {position_, Movement::collisionSize};
}

void Player::refreshContacts(const std::vector<sf::FloatRect>& solids)
{
    grounded_ = false;
    touchingWall_ = false;
    const auto size = Movement::collisionSize;
    for (const auto& solid : solids)
    {
        if (overlaps(position_.x, size.x, solid.position.x, solid.size.x) &&
            std::abs(position_.y + size.y - solid.position.y) <= contactTolerance &&
            velocity_.y >= 0.f)
            grounded_ = true;
        if (overlaps(position_.y, size.y, solid.position.y, solid.size.y) &&
            (std::abs(position_.x + size.x - solid.position.x) <= contactTolerance ||
             std::abs(position_.x - solid.position.x - solid.size.x) <= contactTolerance))
            touchingWall_ = true;
    }
}

void Player::moveAxis(float distance, bool horizontal, const std::vector<sf::FloatRect>& solids)
{
    // Sweep this axis to the nearest face; even a thin solid blocks a fast dash.
    const auto size = Movement::collisionSize;
    const float start = horizontal ? position_.x : position_.y;
    const float extent = horizontal ? size.x : size.y;
    const float other = horizontal ? position_.y : position_.x;
    const float otherExtent = horizontal ? size.y : size.x;
    float allowed = distance;
    for (const auto& solid : solids)
    {
        const float nearFace = horizontal ? solid.position.x : solid.position.y;
        const float solidExtent = horizontal ? solid.size.x : solid.size.y;
        const float otherFace = horizontal ? solid.position.y : solid.position.x;
        const float otherSize = horizontal ? solid.size.y : solid.size.x;
        if (!overlaps(other, otherExtent, otherFace, otherSize))
            continue;
        if (distance > 0.f && start + extent <= nearFace && start + extent + allowed >= nearFace)
            allowed = nearFace - start - extent;
        if (distance < 0.f && start >= nearFace + solidExtent && start + allowed <= nearFace + solidExtent)
            allowed = nearFace + solidExtent - start;
    }
    float& coordinate = horizontal ? position_.x : position_.y;
    float& speed = horizontal ? velocity_.x : velocity_.y;
    coordinate += allowed;
    if (allowed != distance)
        speed = 0.f;
}

void Player::update(const InputIntent& intent, float deltaTime,
                    const std::vector<sf::FloatRect>& solids, float roomWidth)
{
    refreshContacts(solids);
    if (!grounded_ || intent.direction.x != runDirection_)
        runDirection_ = 0.f;
    // Reuse the input double-tap signal; only a held horizontal direction starts Run.
    if (grounded_ && intent.dashDirection.x != 0.f &&
        intent.direction.x == (intent.dashDirection.x > 0.f ? 1.f : -1.f))
    {
        if (runDirection_ == 0.f)
            velocity_.x = intent.direction.x * Movement::moveSpeed;
        runDirection_ = intent.direction.x;
    }
    // Grounded input never starts an air dash, including on the jump frame.
    if (!grounded_ && dashRemaining_ <= 0.f && intent.dashDirection != sf::Vector2f{})
    {
        const float length = std::sqrt(intent.dashDirection.x * intent.dashDirection.x +
                                       intent.dashDirection.y * intent.dashDirection.y);
        velocity_ = intent.dashDirection / length * Movement::dashSpeed;
        dashRemaining_ = Movement::dashDuration;
    }
    if (intent.jumpRequested && (grounded_ || touchingWall_))
    {
        // Wall jumps deliberately have no horizontal kick: repeated wall jumps are allowed.
        dashRemaining_ = 0.f;
        velocity_.y = -Movement::jumpSpeed;
        grounded_ = false;
    }

    float remaining = deltaTime;
    while (remaining > 0.f)
    {
        const bool dashing = dashRemaining_ > 0.f;
        float step = std::min(remaining, maxStep);
        if (dashing)
            step = std::min(step, dashRemaining_);
        else
        {
            // Jumping or walking off an edge restores the existing air control immediately.
            if (!grounded_)
                runDirection_ = 0.f;
            if (runDirection_ != 0.f)
                velocity_.x = runDirection_ * std::min(Movement::runMaxSpeed,
                    std::max(Movement::moveSpeed, std::abs(velocity_.x)) + Movement::runAcceleration * step);
            else
                velocity_.x = intent.direction.x * Movement::moveSpeed;
        }

        velocity_.y += Movement::gravity * (dashing ? Movement::dashGravityScale : 1.f) * step;
        if (!dashing && touchingWall_ && !grounded_ && velocity_.y > Movement::wallSlideSpeed)
            velocity_.y = Movement::wallSlideSpeed;

        moveAxis(velocity_.x * step, true, solids);
        const float clampedX = std::clamp(position_.x, 0.f,
                                          std::max(0.f, roomWidth - Movement::collisionSize.x));
        if (clampedX != position_.x)
            velocity_.x = 0.f;
        position_.x = clampedX;
        // Detect a newly reached wall before integrating the fall this step.
        refreshContacts(solids);
        if (!dashing && touchingWall_ && !grounded_ && velocity_.y > Movement::wallSlideSpeed)
            velocity_.y = Movement::wallSlideSpeed;
        moveAxis(velocity_.y * step, false, solids);
        refreshContacts(solids);
        if (dashing)
            dashRemaining_ = std::max(0.f, dashRemaining_ - step);
        if (grounded_)
            dashRemaining_ = 0.f;
        remaining = std::max(0.f, remaining - step);
    }
}

MovementState Player::state() const
{
    if (dashRemaining_ > 0.f)
        return MovementState::Dashing;
    if (grounded_)
        return runDirection_ != 0.f ? MovementState::Running : MovementState::Grounded;
    if (touchingWall_ && velocity_.y >= 0.f)
        return MovementState::WallSliding;
    return MovementState::Airborne;
}
