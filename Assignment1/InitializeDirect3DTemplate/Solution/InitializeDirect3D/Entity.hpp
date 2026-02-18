#pragma once
#include "SceneNode.hpp"

/**
 * @brief A SceneNode that can move, driven by a velocity vector.
 *
 * Entity extends SceneNode with an mVelocity field and overrides
 * updateCurrent() to translate the node each frame by velocity * deltaTime.
 * It also marks the associated RenderItem dirty so the GPU CB is refreshed.
 *
 * All game objects that move (aircraft, bullets, pickups) should derive
 * from Entity rather than SceneNode directly.
 */
class Entity : public SceneNode
{
public:
    /// @brief Constructs an Entity with zero initial velocity.
    Entity(Game* game);

    /// @brief Sets velocity from an XMFLOAT3.
    void     setVelocity(XMFLOAT3 velocity);

    /// @brief Sets velocity from individual components.
    void     setVelocity(float vx, float vy, float vz);

    /// @brief Returns the current velocity.
    XMFLOAT3 getVelocity() const;

    /**
     * @brief Moves this entity and updates the GPU constant buffer each frame.
     *
     * Applies velocity * deltaTime to the node's position, recomputes the
     * world transform, and increments NumFramesDirty so all frame resources
     * receive the updated matrix.
     *
     * @param gt  Game timer providing DeltaTime().
     */
    virtual void updateCurrent(const GameTimer& gt) override;

protected:
    XMFLOAT3 mVelocity;
};
