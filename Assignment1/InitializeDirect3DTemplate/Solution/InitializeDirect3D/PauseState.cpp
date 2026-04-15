/**
 * @file PauseState.cpp
 * @brief Implementation of the PauseState class.
 */

#include "PauseState.hpp"
#include "StateStack.hpp"
#include "Game.hpp"

/**
 * @brief Constructs the PauseState and updates the window title.
 * @param stack    The owning StateStack.
 * @param context  Shared application context.
 */
PauseState::PauseState(StateStack& stack, Context context)
    : State(stack, context)
{
    SetWindowTextA(context.game->MainWnd(),
        "Aircraft Shooter  |  GAME PAUSED  |  ESC: Resume   BACKSPACE: Main Menu");
}

// ---------------------------------------------------------------------------
// State interface
// ---------------------------------------------------------------------------

/**
 * @brief Draws the pause overlay.
 *
 * GameState::draw() runs first (bottom-to-top draw order) so the 3D
 * scene is visible behind the pause state. PauseState itself adds no
 * geometry — the window title communicates the paused state.
 */
void PauseState::draw()
{
    // No additional geometry — GameState draws the 3D scene below.
    // A full implementation would darken the screen with a semi-transparent
    // quad and render "Game Paused" text over it.
}

/**
 * @brief Blocks GameState from updating while paused.
 *
 * Returning false stops the StateStack from propagating update() to
 * GameState below, effectively freezing the game world.
 *
 * @param gt  Game timer (unused).
 * @return    False — prevents states below from updating.
 */
bool PauseState::update(const GameTimer& gt)
{
    return false;
}

/**
 * @brief Handles resume and return-to-menu key presses.
 *
 * Escape pops this state, resuming GameState.
 * Backspace clears the entire stack and pushes MenuState, returning
 * the player to the main menu without saving game progress.
 *
 * @param key  Virtual-key code of the pressed key.
 * @return     False — consumes the key event.
 */
bool PauseState::handleEvent(WPARAM key)
{
    if (key == VK_ESCAPE)
    {
        // Resume — pop PauseState, GameState resumes updating.
        requestStackPop();
    }
    else if (key == VK_BACK)
    {
        // Return to main menu — discard game progress.
        requestStateClear();
        requestStackPush(States::Menu);
    }

    return false;
}
