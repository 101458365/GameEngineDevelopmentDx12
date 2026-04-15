#pragma once
#include "../../Common/d3dApp.h"
#include "../../Common/GameTimer.h"

class StateStack;
class Game;

/**
 * @brief Shared context passed to every State on construction.
 *
 * Context is a lightweight struct that gives each State access to the
 * core application objects it needs — the Game (for GPU resources) and
 * the GameTimer (for delta time). It avoids passing many individual
 * parameters to every State constructor.
 */
struct Context
{
    /**
     * @brief Constructs a Context with the given application objects.
     * @param game   Pointer to the owning Game instance.
     */
    Context(Game* game)
        : game(game)
    {
    }

    /// Pointer to the owning Game — provides access to GPU resources.
    Game* game;
};

/**
 * @brief Abstract base class for all game states.
 *
 * A State represents one screen or mode of the application — Title,
 * Menu, Game, or Pause. Each state owns its own draw/update/input logic.
 *
 * States live on a StateStack. A state can request stack operations
 * (push, pop, clear) via the protected helper methods, which are queued
 * and applied at the end of each frame to avoid modifying the stack
 * while it is being iterated.
 *
 * Returning true from update() or handleEvent() allows states below on
 * the stack to also update/handle input. Returning false blocks them.
 * This is how PauseState freezes GameState — it returns false from update().
 */
class State
{
public:
    typedef std::unique_ptr<State> Ptr;

public:
    /**
     * @brief Constructs a State with its stack and shared context.
     * @param stack    The StateStack this state lives on.
     * @param context  Shared application context.
     */
    State(StateStack& stack, Context context);

    /// @brief Virtual destructor — States are polymorphic.
    virtual ~State();

    // -------------------------------------------------------
    // Pure virtual interface — every State must implement these
    // -------------------------------------------------------

    /// @brief Draws this state's content for one frame.
    virtual void draw() = 0;

    /**
     * @brief Updates this state for one frame.
     *
     * @param gt  Game timer providing delta time.
     * @return    True to allow states below to also update, false to block them.
     */
    virtual bool update(const GameTimer& gt) = 0;

    /**
     * @brief Handles a single keypress event.
     *
     * Called once per key-down event (not per frame). Use this for
     * one-time actions like confirming a menu selection.
     *
     * @param key     Virtual-key code of the pressed key.
     * @return        True to allow states below to also handle the event.
     */
    virtual bool handleEvent(WPARAM key) = 0;

protected:
    // -------------------------------------------------------
    // Stack manipulation helpers
    // -------------------------------------------------------

    /**
     * @brief Requests that a new state be pushed onto the stack.
     * @param stateID  Identifier of the state to push.
     */
    void requestStackPush(int stateID);

    /// @brief Requests that this state be popped off the stack.
    void requestStackPop();

    /// @brief Requests that the entire stack be cleared.
    void requestStateClear();

    /// @brief Returns the shared application context.
    Context getContext() const;

private:
    /// The stack this state lives on — used to request push/pop/clear.
    StateStack& mStack;

    /// Shared application context (Game pointer etc.).
    Context mContext;
};