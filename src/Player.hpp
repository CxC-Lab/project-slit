#pragma once

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>
#include <vector>

// Current playtest candidates, not final movement specifications.
namespace Movement
{
inline constexpr float moveSpeed = 260.f; // Walking speed, unchanged.
inline constexpr float runMaxSpeed = 440.f;
inline constexpr float runAcceleration = 600.f;
inline constexpr float jumpSpeed = 520.f;
inline constexpr float gravity = 1500.f;
inline constexpr float dashSpeed = 700.f;
inline constexpr float dashDuration = 0.14f;
inline constexpr float doubleTapWindow = 0.25f;
inline constexpr float dashGravityScale = 0.f;
inline constexpr float wallSlideSpeed = 90.f;
inline constexpr float slowFallMaxSpeed = 120.f;
inline constexpr float fastFallAcceleration = 3000.f; // Added to ordinary gravity while descending.
inline constexpr float fastFallMaxSpeed = 1200.f;
inline constexpr float fallMaxSpeed = 1000.f;
inline constexpr float landImpactSpeed = 700.f; // Above ordinary jump landing speed (~520).
inline constexpr sf::Vector2f collisionSize{32.f, 48.f};
}

struct InputIntent
{
    sf::Vector2f direction{};
    bool jumpRequested = false;
    sf::Vector2f dashDirection{}; // Zero means no request; captured at the second tap.
};

enum class MovementState
{
    Idle, Walking, Sprinting, Jumping, Falling, AirDashing, WallSliding, SlowFalling, FastFalling
};
const char* toString(MovementState state);

class Player
{
public:
    explicit Player(sf::Vector2f position = {100.f, 472.f});
    void update(const InputIntent& intent, float deltaTime,
                const std::vector<sf::FloatRect>& solids, float roomWidth);
    sf::FloatRect collisionBounds() const;
    sf::Vector2f velocity() const { return velocity_; }
    bool landImpact() const { return landImpact_; }
    bool grounded() const { return grounded_; }
    bool touchingWall() const { return touchingWall_; }
    MovementState state() const { return state_; }

private:
    void updateState(const InputIntent& intent);
    void limitFallSpeed();
    void refreshContacts(const std::vector<sf::FloatRect>& solids);
    void moveAxis(float distance, bool horizontal, const std::vector<sf::FloatRect>& solids);
    sf::Vector2f position_;
    sf::Vector2f velocity_{};
    bool grounded_ = false;
    bool landImpact_ = false;
    bool touchingWall_ = false;
    float dashRemaining_ = 0.f;
    float runDirection_ = 0.f;
    bool sprintMomentum_ = false;
    MovementState state_ = MovementState::Falling;
};
