/**
 * @file MenuState.cpp
 * @brief Implementation of the MenuState class.
 */

#include "MenuState.hpp"
#include "StateStack.hpp"
#include "Game.hpp"

/**
 * @brief Constructs the MenuState, defaulting selection to Play.
 * @param stack    The owning StateStack.
 * @param context  Shared application context.
 */
MenuState::MenuState(StateStack& stack, Context context)
    : State(stack, context)
    , mSelectedOption(Play)
{
    updateTitle();
}

// ---------------------------------------------------------------------------
// State interface
// ---------------------------------------------------------------------------

/**
 * @brief Clears the screen with the menu colour each frame.
 *
 * The distinct colour makes it clear this is the menu state.
 * A full implementation would render menu option text here.
 */
void MenuState::draw()
{
    // No 3D geometry — visual feedback is via window title and clear colour.
}

/**
 * @brief Updates the menu each frame.
 * @param gt  Game timer (unused — menu is event-driven).
 * @return    True — no states below need blocking.
 */
bool MenuState::update(const GameTimer& gt)
{
    return true;
}

/**
 * @brief Handles menu navigation and selection.
 *
 * UP / DOWN arrow keys move the selection. ENTER confirms it.
 * Play clears the stack and pushes GameState.
 * Exit clears the stack — the empty stack causes the app to exit.
 *
 * @param key  Virtual-key code of the pressed key.
 * @return     False — consumes the key event.
 */
bool MenuState::handleEvent(WPARAM key)
{
    if (key == VK_UP)
    {
        // Move selection up, wrapping around.
        mSelectedOption = (mSelectedOption - 1 + OptionCount) % OptionCount;
        updateTitle();
    }
    else if (key == VK_DOWN)
    {
        // Move selection down, wrapping around.
        mSelectedOption = (mSelectedOption + 1) % OptionCount;
        updateTitle();
    }
    else if (key == VK_RETURN)
    {
        if (mSelectedOption == Play)
        {
            // Clear the menu and start the game.
            requestStackPop();
            requestStackPush(States::Game);
        }
        else if (mSelectedOption == Exit)
        {
            // Empty stack causes Game::Update to post quit message.
            requestStateClear();
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

/**
 * @brief Updates the window title to show the current selection.
 *
 * Uses the title bar as a substitute for on-screen rendered text.
 * Format: "Main Menu  |  [>Play<]  Exit" or "Main Menu  |  Play  [>Exit<]"
 */
void MenuState::updateTitle()
{
    if (mSelectedOption == Play)
        SetWindowTextA(getContext().game->MainWnd(),
            "Aircraft Shooter  |  Main Menu  |  [> Play <]   Exit      (UP/DOWN to navigate, ENTER to select)");
    else
        SetWindowTextA(getContext().game->MainWnd(),
            "Aircraft Shooter  |  Main Menu  |    Play    [> Exit <]   (UP/DOWN to navigate, ENTER to select)");
}
