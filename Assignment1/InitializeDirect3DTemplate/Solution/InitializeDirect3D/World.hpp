#pragma once
#include "SceneNode.hpp"
#include "Aircraft.hpp"

/**
 * @brief Manages the game world: the scene graph and all game objects.
 *
 * World owns the root SceneNode and holds a pointer to the player aircraft
 * so that input handling can apply velocity directly to it each frame.
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
     * @brief Reads arrow-key state and sets the player aircraft velocity.
     *
     * Called every frame by Game::OnKeyboardInput().
     * Left/Right move along X, Up/Down move along Z.
     *
     * @param gt  Game timer providing delta time.
     */
    void handlePlayerInput(const GameTimer& gt);

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

    /// @brief Non-owning pointer to the player's Eagle aircraft.
    Aircraft*                          mPlayerAircraft;

    /// Movement speed of the player aircraft (units per second).
    static constexpr float             PlayerSpeed = 10.0f;
};
