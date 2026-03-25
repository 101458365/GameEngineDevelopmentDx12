#pragma once
#include "SceneNode.hpp"
#include "Aircraft.hpp"
#include "CommandQueue.hpp"

/**
 * @brief Manages the game world: the scene graph and all game objects.
 *
 * World owns the root SceneNode and holds a pointer to the player aircraft.
 *
 * Assignment 2 additions:
 *   - Owns a CommandQueue that Player pushes commands into each frame.
 *   - update() drains the queue and dispatches each command through the
 *     scene graph before the regular node update step.
 *   - getCommandQueue() exposes the queue so Game can pass it to Player.
 *   - handlePlayerInput() is removed — Player now owns that responsibility.
 */
class World
{
public:
    /// @brief Constructs the World with a back-pointer to the Game.
    explicit World(Game* game);

    /// @brief Propagates update through the entire scene graph.
    void update(const GameTimer& gt);

    /// @brief Propagates draw through the entire scene graph.
    void draw();

    /// @brief Builds and attaches all scene nodes (aircraft etc.).
    void buildScene();

    /**
     * @brief Returns a reference to the command queue.
     *
     * Called by Game::processInput() to give Player a place to push commands.
     *
     * @return Reference to the world's CommandQueue.
     */
    CommandQueue& getCommandQueue() { return mCommandQueue; }

private:
    /// @brief Layer indices for organising scene-graph children.
    enum Layer
    {
        Background,
        Air,
        LayerCount
    };

    Game*                              mGame;
    std::unique_ptr<SceneNode>         mSceneGraph;
    std::array<SceneNode*, LayerCount> mSceneLayers;

    /// The command queue — Player pushes in, World pops out each frame.
    CommandQueue                       mCommandQueue;

    /// @brief Non-owning pointer to the player's Eagle aircraft.
    Aircraft*                          mPlayerAircraft;

    /// Movement speed of the player aircraft (units per second).
    static constexpr float             PlayerSpeed = 10.0f;
};
