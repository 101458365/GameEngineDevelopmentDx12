/**
 * @file Entity.cpp
 * @brief Implementation of the Entity class.
 */

#include "Entity.hpp"

/**
 * @brief Constructs an Entity with zero velocity.
 * @param game  Back-pointer to the owning Game.
 */
Entity::Entity(Game* game)
    : SceneNode(game)
    , mVelocity(0.0f, 0.0f, 0.0f)
{
}

// ---------------------------------------------------------------------------
// Velocity accessors
// ---------------------------------------------------------------------------

/**
 * @brief Sets the velocity from an XMFLOAT3.
 * @param velocity  New velocity in units per second.
 */
void Entity::setVelocity(XMFLOAT3 velocity)
{
    mVelocity = velocity;
}

/**
 * @brief Sets the velocity from individual float components.
 * @param vx, vy, vz  Velocity in X, Y, Z axes (units per second).
 */
void Entity::setVelocity(float vx, float vy, float vz)
{
    mVelocity.x = vx;
    mVelocity.y = vy;
    mVelocity.z = vz;
}

/**
 * @brief Returns the current velocity.
 * @return Velocity in units per second.
 */
XMFLOAT3 Entity::getVelocity() const
{
    return mVelocity;
}

// ---------------------------------------------------------------------------
// Per-frame update
// ---------------------------------------------------------------------------

/**
 * @brief Moves this entity by velocity * deltaTime and marks the CB dirty.
 *
 * If renderer is null (the node has not been built yet, or is a non-renderable
 * intermediate node) the CB update is safely skipped.
 *
 * @param gt  Game timer providing DeltaTime().
 */
void Entity::updateCurrent(const GameTimer& gt)
{
    const float dt = gt.DeltaTime();

    // Translate by velocity * deltaTime.
    move(mVelocity.x * dt,
         mVelocity.y * dt,
         mVelocity.z * dt);

    // Push the new world transform to the GPU constant buffer.
    // Guard against non-renderable entity nodes (renderer == nullptr).
    if (renderer != nullptr)
    {
        renderer->World = getWorldTransform();
        renderer->NumFramesDirty++;
    }
}
