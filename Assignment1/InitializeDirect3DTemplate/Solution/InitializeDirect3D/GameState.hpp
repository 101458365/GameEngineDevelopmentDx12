#pragma once
#include "State.hpp"
#include "World.hpp"
#include "Player.hpp"

/**
 * @brief The game screen — the 3D aircraft scene from Assignments 1 and 2.
 *
 * GameState wraps the World and Player objects that were previously owned
 * directly by Game. By moving them here, the game logic only runs when
 * GameState is on the stack, and it is automatically frozen when PauseState
 * is pushed on top (because PauseState::update() returns false).
 *
 * State flow:
 *   Escape pressed → push PauseState on top (GameState stays on stack)
 *
 * update() returns false so states below GameState (none normally) cannot
 * update. handleEvent() returns false to consume the Escape key.
 */
class GameState : public State
{
public:
    /**
     * @brief Constructs the GameState and builds the scene.
     *
     * Creates the World (which builds the scene graph and registers render
     * items) and the Player (which sets up key bindings and commands).
     *
     * @param stack    The owning StateStack.
     * @param context  Shared application context.
     */
    GameState(StateStack& stack, Context context);

    /// @brief Clears mOpaqueRitems so Draw() stops rendering 3D geometry on exit.
    virtual ~GameState();

    /// @brief Delegates draw to the World scene graph.
    virtual void draw() override;

    /**
     * @brief Updates the World and Player command system each frame.
     *
     * Processes player input via the command queue, then updates the
     * scene graph (applies velocities, updates GPU constant buffers).
     *
     * @param gt  Game timer providing delta time.
     * @return    False — blocks states below from updating.
     */
    virtual bool update(const GameTimer& gt) override;

    /**
     * @brief Handles one-time key events for the game screen.
     *
     * Escape pushes PauseState. All other keys are forwarded to the
     * Player's handleEvent() for discrete actions (e.g. firing).
     *
     * @param key  Virtual-key code of the pressed key.
     * @return     False — consumes the key event.
     */
    virtual bool handleEvent(WPARAM key) override;

private:
    /// The game world — owns the scene graph, aircraft, and floor.
    World  mWorld;

    /// The player input handler — owns key bindings and command creation.
    Player mPlayer;
};