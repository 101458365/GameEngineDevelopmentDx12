/**
 * @file TitleState.cpp
 * @brief Implementation of the TitleState class.
 */

#include "TitleState.hpp"
#include "StateStack.hpp"
#include "Game.hpp"

/**
 * @brief Constructs TitleState and sets the window title.
 * @param stack    The owning StateStack.
 * @param context  Shared application context.
 */
TitleState::TitleState(StateStack& stack, Context context)
    : State(stack, context)
{
    // Use the window title bar as a cheap substitute for on-screen text
    // since DirectX 12 has no built-in 2D font renderer.
    SetWindowTextA(getContext().game->MainWnd(), "Aircraft Shooter  |  Press Any Key To Start");
}

// ---------------------------------------------------------------------------
// State interface
// ---------------------------------------------------------------------------

/**
 * @brief Clears the screen with a dark navy colour each frame.
 *
 * The distinct colour makes it visually obvious which state is active.
 * In a full implementation this would render a background image and
 * on-screen text via a sprite batch or ImGui.
 */
void TitleState::draw()
{
    // Drawing is handled by Game::Draw() which calls StateStack::draw().
    // The clear colour is set inside Game::Draw() based on current state.
    // TitleState itself has no 3D geometry to submit.
}

/**
 * @brief Updates the title screen.
 *
 * The title screen is static so no per-frame logic is needed.
 *
 * @param gt  Game timer (unused).
 * @return    True — allows states below to update (none exist here).
 */
bool TitleState::update(const GameTimer& gt)
{
    return true;
}

/**
 * @brief Transitions to MenuState on any key press.
 *
 * Any virtual-key code is accepted. Pops this state and pushes
 * MenuState so the player proceeds to the main menu.
 *
 * @param key  Virtual-key code (ignored — any key accepted).
 * @return     False — consumes the event.
 */
bool TitleState::handleEvent(WPARAM key)
{
    // Any key proceeds to the main menu.
    requestStackPop();
    requestStackPush(States::Menu);
    return false;
}
