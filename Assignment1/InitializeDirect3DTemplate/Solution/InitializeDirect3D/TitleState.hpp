#pragma once
#include "State.hpp"

/**
 * @brief The title screen — the first thing the player sees.
 *
 * TitleState displays a solid colour background with the window title
 * set to "Press Any Key To Start". Since DirectX 12 does not provide
 * built-in 2D text rendering, the state communicates via the window
 * title bar and background clear colour rather than on-screen text.
 *
 * State flow:
 *   Any key pressed → pop TitleState → push MenuState
 *
 * update() returns true so any states below can still update (there
 * are none in practice, but it keeps the contract consistent).
 * handleEvent() returns false once a key is pressed to consume the
 * event and prevent it reaching states below.
 */
class TitleState : public State
{
public:
    /**
     * @brief Constructs the TitleState.
     * @param stack    The owning StateStack.
     * @param context  Shared application context.
     */
    TitleState(StateStack& stack, Context context);

    /// @brief Clears the screen with the title colour (dark navy).
    virtual void draw() override;

    /**
     * @brief Updates the title screen each frame.
     * @param gt  Game timer (unused — title screen is static).
     * @return    True to allow states below to update.
     */
    virtual bool update(const GameTimer& gt) override;

    /**
     * @brief Transitions to MenuState on any key press.
     *
     * Pops this state and pushes MenuState so the player proceeds
     * to the main menu.
     *
     * @param key  Virtual-key code of the pressed key (any key accepted).
     * @return     False — consumes the event.
     */
    virtual bool handleEvent(WPARAM key) override;
};