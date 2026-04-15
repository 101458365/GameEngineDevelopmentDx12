/**
 * @file State.cpp
 * @brief Implementation of the State base class.
 */

#include "State.hpp"
#include "StateStack.hpp"

/**
 * @brief Constructs a State with its owning stack and shared context.
 * @param stack    The StateStack this state lives on.
 * @param context  Shared application context.
 */
State::State(StateStack& stack, Context context)
    : mStack(stack)
    , mContext(context)
{
}

State::~State()
{
}

// ---------------------------------------------------------------------------
// Stack manipulation helpers
// ---------------------------------------------------------------------------

/**
 * @brief Requests that a new state be pushed onto the stack.
 *
 * The push is queued and applied at the end of the current frame.
 *
 * @param stateID  The States::ID of the state to push.
 */
void State::requestStackPush(int stateID)
{
    mStack.pushState(static_cast<States::ID>(stateID));
}

/**
 * @brief Requests that this state be popped off the stack.
 *
 * The pop is queued and applied at the end of the current frame.
 */
void State::requestStackPop()
{
    mStack.popState();
}

/**
 * @brief Requests that the entire stack be cleared.
 *
 * The clear is queued and applied at the end of the current frame.
 * Useful for returning to the main menu from anywhere.
 */
void State::requestStateClear()
{
    mStack.clearStates();
}

/**
 * @brief Returns the shared application context.
 * @return The Context holding the Game pointer.
 */
Context State::getContext() const
{
    return mContext;
}
