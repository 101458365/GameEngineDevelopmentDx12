/**
 * @file StateStack.cpp
 * @brief Implementation of the StateStack class.
 */

#include "StateStack.hpp"

/**
 * @brief Constructs the StateStack with a shared Context.
 * @param context  Application context passed to every new State.
 */
StateStack::StateStack(Context context)
    : mStack()
    , mPendingList()
    , mContext(context)
    , mFactories()
{
}

// ---------------------------------------------------------------------------
// Per-frame interface
// ---------------------------------------------------------------------------

/**
 * @brief Updates active states top-to-bottom until one returns false.
 *
 * Iterates from the top of the stack downward. If a state returns false
 * from update(), iteration stops — states below are blocked. This is how
 * PauseState freezes GameState without destroying it.
 *
 * Pending stack changes are applied after iteration is complete.
 *
 * @param gt  Game timer providing delta time.
 */
void StateStack::update(const GameTimer& gt)
{
    for (auto it = mStack.rbegin(); it != mStack.rend(); ++it)
    {
        if (!(*it)->update(gt))
            break;
    }
    applyPendingChanges();
}

/**
 * @brief Draws all active states bottom-to-top.
 *
 * Drawing goes from bottom to top so upper states (e.g. PauseState)
 * render over lower ones (e.g. GameState).
 */
void StateStack::draw()
{
    for (auto& state : mStack)
        state->draw();
}

/**
 * @brief Passes a key event to active states top-to-bottom.
 *
 * Stops propagating if a state returns false from handleEvent().
 *
 * @param key  Virtual-key code of the pressed key.
 */
void StateStack::handleEvent(WPARAM key)
{
    for (auto it = mStack.rbegin(); it != mStack.rend(); ++it)
    {
        if (!(*it)->handleEvent(key))
            break;
    }
    applyPendingChanges();
}

// ---------------------------------------------------------------------------
// Stack operations
// ---------------------------------------------------------------------------

/**
 * @brief Queues a push of the state with the given ID.
 * @param stateID  The States::ID to push.
 */
void StateStack::pushState(States::ID stateID)
{
    mPendingList.push_back(PendingChange(Push, stateID));
}

/**
 * @brief Queues a pop of the top state.
 */
void StateStack::popState()
{
    mPendingList.push_back(PendingChange(Pop));
}

/**
 * @brief Queues a clear of the entire stack.
 */
void StateStack::clearStates()
{
    mPendingList.push_back(PendingChange(Clear));
}

/**
 * @brief Returns true if the stack has no active states.
 * @return True when empty.
 */
bool StateStack::isEmpty() const
{
    return mStack.empty();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

/**
 * @brief Applies all queued push/pop/clear operations.
 *
 * Called at the end of update() and handleEvent() after iteration is
 * complete, so the stack is never modified while being traversed.
 */
void StateStack::applyPendingChanges()
{
    for (auto& change : mPendingList)
    {
        switch (change.action)
        {
        case Push:
            mStack.push_back(createState(change.stateID));
            break;

        case Pop:
            mStack.pop_back();
            break;

        case Clear:
            mStack.clear();
            break;
        }
    }
    mPendingList.clear();
}

/**
 * @brief Constructs and returns a new State for the given ID.
 *
 * Looks up the factory lambda registered for this ID and calls it.
 * Asserts if the ID has not been registered.
 *
 * @param stateID  The States::ID to construct.
 * @return         Owning pointer to the new State.
 */
State::Ptr StateStack::createState(States::ID stateID)
{
    auto found = mFactories.find(stateID);
    assert(found != mFactories.end());
    return found->second();
}
