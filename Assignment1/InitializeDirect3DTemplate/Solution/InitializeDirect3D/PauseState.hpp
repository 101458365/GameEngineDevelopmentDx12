#pragma once
#include "State.hpp"

/**
 * @brief The pause screen — overlaid on top of GameState.
 *
 * PauseState sits on top of GameState on the stack. Because its update()
 * returns false, GameState::update() is blocked — the 3D scene freezes.
 * However GameState::draw() still runs (draw goes bottom-to-top), so the
 * game world remains visible behind the pause overlay.
 *
 * Controls:
 *   ESCAPE     — resume game (pop PauseState, GameState resumes)
 *   BACKSPACE  — return to main menu (clear stack, push MenuState)
 *
 * handleEvent() returns false to consume both keys.
 */
class PauseState : public State
{
public:
    /**
     * @brief Constructs the PauseState and updates the window title.
     * @param stack    The owning StateStack.
     * @param context  Shared application context.
     */
    PauseState(StateStack& stack, Context context);

    /**
     * @brief Draws the pause overlay.
     *
     * The 3D game scene is still drawn by GameState below. PauseState
     * itself has no additional geometry — the window title communicates
     * the paused state to the player.
     */
    virtual void draw() override;

    /**
     * @brief Blocks GameState from updating while paused.
     * @param gt  Game timer (unused).
     * @return    False — prevents states below from updating.
     */
    virtual bool update(const GameTimer& gt) override;

    /**
     * @brief Handles Escape (resume) and Backspace (main menu).
     * @param key  Virtual-key code of the pressed key.
     * @return     False — consumes the key event.
     */
    virtual bool handleEvent(WPARAM key) override;
};
