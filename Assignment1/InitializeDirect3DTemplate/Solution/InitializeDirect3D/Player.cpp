/**
 * @file Player.cpp
 * @brief Implementation of the Player class.
 */

#include "Player.hpp"
#include "Aircraft.hpp"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

/**
 * @brief Sets up default key bindings and initialises action commands.
 *
 * Arrow keys and WASD are both bound by default. Actions are mapped to
 * AircraftMover functors that accelerate the player aircraft each frame.
 */
Player::Player()
{
    // Default key bindings — arrow keys
    mKeyBinding[VK_LEFT]  = MoveLeft;
    mKeyBinding[VK_RIGHT] = MoveRight;
    mKeyBinding[VK_UP]    = MoveForward;
    mKeyBinding[VK_DOWN]  = MoveBackward;

    // WASD as alternates
    /*mKeyBinding['A'] = MoveLeft;
    mKeyBinding['D'] = MoveRight;
    mKeyBinding['W'] = MoveForward;
    mKeyBinding['S'] = MoveBackward;*/

    initializeActions();

    // All movement commands target only the player aircraft
    for (auto& pair : mActionBinding)
        pair.second.category = Category::PlayerAircraft;
}

// ---------------------------------------------------------------------------
// Action initialisation
// ---------------------------------------------------------------------------

/**
 * @brief Populates mActionBinding with AircraftMover commands.
 *
 * Each action maps to a derivedAction<Aircraft> that calls accelerate()
 * on the player aircraft with the appropriate velocity delta.
 */
void Player::initializeActions()
{
    mActionBinding[MoveLeft].action     = derivedAction<Aircraft>(AircraftMover(-PlayerSpeed, 0.f, 0.f));
    mActionBinding[MoveRight].action    = derivedAction<Aircraft>(AircraftMover( PlayerSpeed, 0.f, 0.f));
    mActionBinding[MoveForward].action  = derivedAction<Aircraft>(AircraftMover(0.f, 0.f,  PlayerSpeed));
    mActionBinding[MoveBackward].action = derivedAction<Aircraft>(AircraftMover(0.f, 0.f, -PlayerSpeed));
}

// ---------------------------------------------------------------------------
// Input handling
// ---------------------------------------------------------------------------

/**
 * @brief Handles one-time input events each frame.
 *
 * Currently a stub — extend here for discrete actions such as firing a
 * weapon on a single key press (as opposed to holding a key).
 *
 * @param commands  The CommandQueue to push commands into.
 * @param gt        Game timer.
 */
void Player::handleEvent(CommandQueue& commands, const GameTimer& gt)
{
    // Example one-time event stub (extend as needed):
    // if (GetAsyncKeyState(VK_SPACE) & 1)
    // {
    //     Command fire;
    //     fire.category = Category::PlayerAircraft;
    //     fire.action   = derivedAction<Aircraft>([](Aircraft& a, const GameTimer&) { a.fire(); });
    //     commands.push(fire);
    // }
}

/**
 * @brief Checks held keys and pushes movement commands each frame.
 *
 * Iterates mKeyBinding and for each key that is currently held and mapped
 * to a realtime action, pushes the corresponding command to the queue.
 *
 * @param commands  The CommandQueue to push commands into.
 * @param gt        Game timer (unused; velocity is applied in Entity::updateCurrent).
 */
void Player::handleRealtimeInput(CommandQueue& commands, const GameTimer& gt)
{
    for (auto& pair : mKeyBinding)
    {
        if (GetAsyncKeyState(pair.first) & 0x8000
            && isRealtimeAction(pair.second))
        {
            commands.push(mActionBinding[pair.second]);
        }
    }
}

// ---------------------------------------------------------------------------
// Key binding management
// ---------------------------------------------------------------------------

/**
 * @brief Reassigns a key to the given action.
 *
 * Removes any existing binding for the action before adding the new one,
 * ensuring no two keys map to the same action simultaneously.
 *
 * @param action  The action to rebind.
 * @param key     Virtual-key code to bind to the action.
 */
void Player::assignKey(Action action, int key)
{
    // Remove old binding for this action if it exists
    for (auto it = mKeyBinding.begin(); it != mKeyBinding.end(); )
    {
        if (it->second == action)
            it = mKeyBinding.erase(it);
        else
            ++it;
    }
    mKeyBinding[key] = action;
}

/**
 * @brief Returns the virtual-key code currently bound to an action.
 * @param action  The action to look up.
 * @return        The bound virtual-key code, or -1 if no binding exists.
 */
int Player::getAssignedKey(Action action) const
{
    for (auto& pair : mKeyBinding)
        if (pair.second == action)
            return pair.first;
    return -1;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * @brief Returns true if the action is driven by held-key (realtime) input.
 *
 * All movement actions are realtime. Add non-realtime actions (e.g. Fire)
 * here by returning false for them so they are handled in handleEvent() instead.
 *
 * @param action  The action to classify.
 * @return        True for all current movement actions.
 */
bool Player::isRealtimeAction(Action action)
{
    switch (action)
    {
    case MoveLeft:
    case MoveRight:
    case MoveForward:
    case MoveBackward:
        return true;
    default:
        return false;
    }
}
