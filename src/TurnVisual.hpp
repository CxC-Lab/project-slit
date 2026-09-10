#pragma once

#include "AnimationClip.hpp"

// Rendering-only direction and one-shot override. No Player or MovementState ownership.
struct TurnVisual
{
    bool facingLeft = false;
    bool turning = false;
    bool requestedLeft = false;

    bool update(float horizontalIntent, double deltaTime, AnimationClip& clip)
    {
        if (horizontalIntent != 0.f)
            requestedLeft = horizontalIntent < 0.f;
        if (turning)
        {
            clip.advance(deltaTime);
            if (!clip.finished())
                return false; // Finish this turn; retain only the latest nonzero intent.
            turning = false;
        }
        if (requestedLeft == facingLeft)
            return false;
        // The atlas itself turns left -> right; mirror the whole clip for a left destination.
        facingLeft = requestedLeft;
        clip.reset();
        turning = true;
        return true;
    }
};
