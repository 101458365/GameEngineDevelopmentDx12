#pragma once
#include "State.hpp"
#include <vector>
#include <functional>
#include <map>

/**
 * @brief State identifiers used to register and request states.
 *
 * Each value uniquely identifies one state class. States request
 * transitions by passing these IDs to requestStackPush().
 */
namespace States
{
    enum ID
    {
        None,   ///< No state — used as a default/null value.
        Title,  ///< TitleState — "Press any key to start".
        Menu,   ///< MenuState  — Play / Exit options.
        Game,   ///< GameState  — the actual 3D aircraft game.
        Pause   ///< PauseState — pause overlay over GameState.
    };
}

/**
 * @brief Manages a stack of active game States.
 *
 * StateStack controls which states are active and in what order they
 * run. States are layered — the top state draws and handles input first.
 * Multiple states can be active simultaneously (e.g. PauseState on top
 * of GameState), which allows the pause overlay to appear over the
 * frozen game scene.
 *
 * Stack operations (push, pop, clear) are queued during the frame and
 * applied at the end of update() to avoid modifying the container
 * while iterating over it.
 *
 * States are created via a factory map: each States::ID is registered
 * with a lambda that constructs the corresponding State subclass.
 */
class StateStack
{
public:
    /**
     * @brief Pending stack operation types.
     */
    enum Action
    {
        Push,   ///< Push a new state onto the stack.
        Pop,    ///< Pop the top state off the stack.
        Clear,  ///< Remove all states from the stack.
    };

public:
    /**
     * @brief Constructs the StateStack with a shared Context.
     * @param context  Application context passed to every new State.
     */
    explicit StateStack(Context context);

    /**
     * @brief Registers a State subclass with the given ID.
     *
     * Stores a factory lambda so the stack can construct the state
     * by ID without needing to know the concrete type.
     *
     * @tparam T       The State subclass to register.
     * @param  stateID The ID to associate with this state.
     */
    template <typename T>
    void registerState(States::ID stateID)
    {
        mFactories[stateID] = [this]()
            {
                return State::Ptr(new T(*this, mContext));
            };
    }

    // -------------------------------------------------------
    // Per-frame interface
    // -------------------------------------------------------

    /// @brief Updates all active states top-to-bottom until one returns false.
    void update(const GameTimer& gt);

    /// @brief Draws all active states bottom-to-top.
    void draw();

    /**
     * @brief Passes a key event to active states top-to-bottom.
     * @param key  Virtual-key code of the pressed key.
     */
    void handleEvent(WPARAM key);

    // -------------------------------------------------------
    // Stack operations (queued, applied at end of update)
    // -------------------------------------------------------

    /// @brief Queues a push of the state with the given ID.
    void pushState(States::ID stateID);

    /// @brief Queues a pop of the top state.
    void popState();

    /// @brief Queues a clear of the entire stack.
    void clearStates();

    /// @brief Returns true if the stack has no active states.
    bool isEmpty() const;

private:
    /// @brief Applies all queued push/pop/clear operations.
    void applyPendingChanges();

    /// @brief Constructs and returns a new State for the given ID.
    State::Ptr createState(States::ID stateID);

private:
    /// Active states — back of vector is the top of the stack.
    std::vector<State::Ptr> mStack;

    /// Pending operations queued during the frame.
    struct PendingChange
    {
        Action     action;
        States::ID stateID;
        PendingChange(Action a, States::ID id = States::None)
            : action(a), stateID(id) {
        }
    };
    std::vector<PendingChange> mPendingList;

    /// Shared context passed to every new State.
    Context mContext;

    /// Factory map: States::ID → construction lambda.
    std::map<States::ID, std::function<State::Ptr()>> mFactories;
};