#pragma once
#include <functional>
#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"

class SceneNode;
class Aircraft;

// ---------------------------------------------------------------------------
// Category namespace
// ---------------------------------------------------------------------------

/**
 * @brief Bitmask categories used to route Commands to the correct scene nodes.
 *
 * Each value has exactly one bit set so categories can be combined with
 * the bitwise OR operator. A node may belong to multiple categories.
 *
 * Example: size_t any = Category::PlayerAircraft | Category::EnemyAircraft;
 */
namespace Category
{
    enum Type
    {
        None           = 0,       ///< No category — node receives no commands.
        Scene          = 1 << 0,  ///< Generic scene node (default).
        PlayerAircraft = 1 << 1,  ///< The player-controlled Eagle.
        AlliedAircraft = 1 << 2,  ///< Friendly AI aircraft.
        EnemyAircraft  = 1 << 3,  ///< Enemy Raptor aircraft.
    };
}

// ---------------------------------------------------------------------------
// Command
// ---------------------------------------------------------------------------

/**
 * @brief A message that can be sent to any scene node matching its category.
 *
 * A Command pairs an action (a callable taking a SceneNode& and GameTimer)
 * with a receiver category bitmask. When dispatched through the scene graph
 * via SceneNode::onCommand(), any node whose category overlaps the command's
 * category will execute the action.
 *
 * This decouples input handling from game logic: the sender does not need
 * to know which specific object will receive the command.
 */
struct Command
{
    /// @brief Default constructor — no action, no category.
    Command() : category(Category::None) {}

    /// @brief The function to execute on a matching scene node.
    std::function<void(SceneNode&, const GameTimer&)> action;

    /// @brief Bitmask of Category::Type values that will receive this command.
    unsigned int category;
};

// ---------------------------------------------------------------------------
// derivedAction helper
// ---------------------------------------------------------------------------

/**
 * @brief Wraps a function that operates on a derived SceneNode type.
 *
 * Commands store functions that operate on SceneNode&. This helper performs
 * a static_cast to the desired derived type before calling the function,
 * so callers can write actions that work directly with Aircraft& etc.
 *
 * @tparam GameObject  The derived SceneNode type to cast to.
 * @tparam Function    A callable accepting (GameObject&, const GameTimer&).
 * @param  fn          The function to wrap.
 * @return             A std::function<void(SceneNode&, const GameTimer&)>.
 */
template <typename GameObject, typename Function>
std::function<void(SceneNode&, const GameTimer&)> derivedAction(Function fn)
{
    return [=](SceneNode& node, const GameTimer& gt)
    {
        fn(static_cast<GameObject&>(node), gt);
    };
}

// ---------------------------------------------------------------------------
// AircraftMover functor
// ---------------------------------------------------------------------------

/**
 * @brief Functor that accelerates an Aircraft by a fixed velocity delta.
 *
 * Used as the action payload inside movement Commands. When invoked,
 * it casts the target SceneNode to Aircraft and calls accelerate().
 *
 * Example usage:
 * @code
 *   Command moveLeft;
 *   moveLeft.category = Category::PlayerAircraft;
 *   moveLeft.action   = derivedAction<Aircraft>(AircraftMover(-speed, 0, 0));
 * @endcode
 */
struct AircraftMover
{
    /**
     * @brief Constructs an AircraftMover with a velocity delta.
     * @param vx  X-axis velocity to add (units/second).
     * @param vy  Y-axis velocity to add (units/second).
     * @param vz  Z-axis velocity to add (units/second).
     */
    AircraftMover(float vx, float vy, float vz)
        : velocity(vx, vy, vz)
    {}

    /**
     * @brief Applies the velocity delta to the target aircraft.
     * @param node  The scene node — must be an Aircraft.
     * @param gt    Game timer (unused; velocity is applied in Entity::updateCurrent).
     */
    void operator()(SceneNode& node, const GameTimer& gt) const;

    /// Velocity delta added to the aircraft each frame this command is active.
    DirectX::XMFLOAT3 velocity;
};
