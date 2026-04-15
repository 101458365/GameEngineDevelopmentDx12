#pragma once
#include "State.hpp"

/**
 * @brief The main menu screen — Play or Exit.
 *
 * MenuState presents the player with two options. Since DirectX 12 has
 * no built-in 2D text, the current selection is shown via the window
 * title bar and the background clear colour is distinct from other states.
 *
 * Navigation:
 *   UP / DOWN arrow  — move selection between Play and Exit
 *   ENTER            — confirm selection
 *   Play selected    → clear stack, push GameState
 *   Exit selected    → clear stack (app exits when stack is empty)
 *
 * update() returns true (no states below need blocking).
 * handleEvent() returns false to consume navigation keys.
 */
class MenuState : public State
{
public:
    /**
     * @brief Menu option indices.
     */
    enum OptionIndex
    {
        Play = 0,
        Exit = 1,
        OptionCount
    };

public:
    /**
     * @brief Constructs the MenuState and sets the window title.
     * @param stack    The owning StateStack.
     * @param context  Shared application context.
     */
    MenuState(StateStack& stack, Context context);

    /// @brief Clears the screen with the menu colour (dark teal).
    virtual void draw() override;

    /**
     * @brief Updates the menu each frame.
     * @param gt  Game timer (unused).
     * @return    True — no states below need blocking.
     */
    virtual bool update(const GameTimer& gt) override;

    /**
     * @brief Handles UP/DOWN navigation and ENTER confirmation.
     * @param key  Virtual-key code of the pressed key.
     * @return     False — consumes navigation keys.
     */
    virtual bool handleEvent(WPARAM key) override;

private:
    /// @brief Updates the window title to reflect the current selection.
    void updateTitle();

private:
    /// Index of the currently highlighted menu option.
    int mSelectedOption;
};
