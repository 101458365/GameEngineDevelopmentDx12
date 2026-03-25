#pragma once
#include "Command.hpp"
#include <queue>

/**
 * @brief FIFO queue of Commands bridging input handling and game logic.
 *
 * CommandQueue is a thin wrapper around std::queue<Command>. It sits between
 * the Player (which pushes commands based on input) and the World (which pops
 * and dispatches them to the scene graph each frame).
 *
 * Data flow:
 *   Player → push() → CommandQueue → pop() → World → SceneNode::onCommand()
 */
class CommandQueue
{
public:
    /**
     * @brief Adds a command to the back of the queue.
     * @param command  The command to enqueue.
     */
    void push(const Command& command) { mQueue.push(command); }

    /**
     * @brief Removes and returns the command at the front of the queue.
     * @return The oldest command in the queue.
     */
    Command pop()
    {
        Command c = mQueue.front();
        mQueue.pop();
        return c;
    }

    /**
     * @brief Returns true if the queue contains no commands.
     * @return True when empty, false otherwise.
     */
    bool isEmpty() const { return mQueue.empty(); }

private:
    /// Internal FIFO container.
    std::queue<Command> mQueue;
};
