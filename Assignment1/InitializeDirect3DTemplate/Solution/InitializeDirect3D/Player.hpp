#pragma once
#include "CommandQueue.hpp"
#include "../../Common/GameTimer.h"
#include <map>

/**
 * @brief Handles all player input and translates it into Commands.
 *
 * Player owns two maps:
 *   - mKeyBinding:    virtual-key code → Action enum
 *   - mActionBinding: Action enum      → Command
 *
 * Each frame Game calls processInput() which calls:
 *   - handleEvent()         for one-time keypresses (e.g. fire on tap)
 *   - handleRealtimeInput() for held keys (e.g. move while held)
 *
 * Both methods push Commands into the CommandQueue, which World drains
 * and dispatches to the scene graph via SceneNode::onCommand().
 *
 * Key bindings can be reassigned at runtime via assignKey().
 */
class Player
{
public:
    /**
     * @brief Actions the player can perform, independent of which key triggers them.
     *
     * Separating actions from keys allows runtime key remapping without
     * changing any game logic.
     */
    enum Action
    {
        MoveLeft,     ///< Move the player aircraft left  (-X).
        MoveRight,    ///< Move the player aircraft right (+X).
        MoveForward,  ///< Move the player aircraft forward (+Z).
        MoveBackward, ///< Move the player aircraft backward (-Z).
        ActionCount   ///< Total number of actions — keep last.
    };

public:
    /**
     * @brief Constructs Player with default arrow-key and WASD bindings.
     *
     * Default bindings:
     *   VK_LEFT  / A → MoveLeft
     *   VK_RIGHT / D → MoveRight
     *   VK_UP    / W → MoveForward
     *   VK_DOWN  / S → MoveBackward
     */
    Player();

    /**
     * @brief Handles one-time input events (key-down, mouse click, etc.).
     *
     * Called once per frame from Game::processInput(). Pushes commands for
     * actions that should fire exactly once per press, not while held.
     *
     * @param commands  The world's command queue to push into.
     * @param gt        Game timer for time-based actions.
     */
    void handleEvent(CommandQueue& commands, const GameTimer& gt);

    /**
     * @brief Handles real-time (held-key) input.
     *
     * Called once per frame from Game::processInput(). Checks
     * GetAsyncKeyState for each bound key and pushes a movement command
     * if the key is currently held.
     *
     * @param commands  The world's command queue to push into.
     * @param gt        Game timer.
     */
    void handleRealtimeInput(CommandQueue& commands, const GameTimer& gt);

    /**
     * @brief Reassigns a key to a different action.
     *
     * Removes any existing binding for the action first so no two keys
     * map to the same action simultaneously.
     *
     * @param action  The action to rebind.
     * @param key     The virtual-key code (e.g. VK_LEFT, 'A').
     */
    void assignKey(Action action, int key);

    /**
     * @brief Returns the virtual-key code currently bound to an action.
     * @param action  The action to look up.
     * @return        The virtual-key code, or -1 if not bound.
     */
    int getAssignedKey(Action action) const;

private:
    /// @brief Populates mActionBinding with default AircraftMover commands.
    void initializeActions();

    /**
     * @brief Returns true if the action is driven by held-key (realtime) input.
     * @param action  The action to classify.
     * @return        True for movement actions, false for discrete events.
     */
    static bool isRealtimeAction(Action action);

private:
    /// Maps virtual-key codes to player actions.
    std::map<int, Action>     mKeyBinding;

    /// Maps player actions to their associated Commands.
    std::map<Action, Command> mActionBinding;

    /// Player aircraft movement speed in units per second.
    static constexpr float    PlayerSpeed = 30.0f;
};
