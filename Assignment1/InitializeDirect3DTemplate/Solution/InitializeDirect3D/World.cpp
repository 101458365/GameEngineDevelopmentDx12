/**
 * @file World.cpp
 * @brief Implementation of the World class.
 *
 * World builds and manages the scene graph. It creates the player Eagle and
 * two enemy Raptors, then delegates update/draw to the root SceneNode each frame.
 * Player movement is handled via handlePlayerInput(), which is called from
 * Game::OnKeyboardInput() every frame.
 */

#include "World.hpp"
#include "Game.hpp"

/**
 * @brief Constructs the World.
 * @param game  Pointer to the owning Game instance.
 */
World::World(Game* game)
    : mGame(game)
    , mSceneGraph(nullptr)
    , mSceneLayers()
    , mPlayerAircraft(nullptr)
{
}

// ---------------------------------------------------------------------------
// Per-frame
// ---------------------------------------------------------------------------

/**
 * @brief Updates the entire scene graph for one frame.
 * @param gt  Game timer.
 */
void World::update(const GameTimer& gt)
{
    mSceneGraph->update(gt);
}

/**
 * @brief Draws the entire scene graph.
 *
 * In this architecture drawing is handled by Game::DrawRenderItems() using
 * the pre-built render-item list, so this call is a no-op but kept for
 * architectural completeness (e.g. future HUD overlays).
 */
void World::draw()
{
    mSceneGraph->draw();
}

// ---------------------------------------------------------------------------
// Player input
// ---------------------------------------------------------------------------

/**
 * @brief Reads arrow-key state and applies velocity to the player aircraft.
 *
 * Velocity is reset to zero each frame so the aircraft stops when no key
 * is pressed. The scene-graph update propagates the velocity into position.
 *
 * Controls:
 *   UP    – move forward  (+Z)
 *   DOWN  – move backward (-Z)
 *   LEFT  – strafe left   (-X)
 *   RIGHT – strafe right  (+X)
 *
 * @param gt  Game timer (currently unused; speed is a constant).
 */
void World::handlePlayerInput(const GameTimer& gt)
{
    if (mPlayerAircraft == nullptr) return;

    float vx = 0.0f, vz = 0.0f;

    if (GetAsyncKeyState(VK_LEFT)  & 0x8000) vx = -PlayerSpeed;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) vx =  PlayerSpeed;
    if (GetAsyncKeyState(VK_UP)    & 0x8000) vz =  PlayerSpeed;
    if (GetAsyncKeyState(VK_DOWN)  & 0x8000) vz = -PlayerSpeed;

    mPlayerAircraft->setVelocity(vx, 0.0f, vz);
}

// ---------------------------------------------------------------------------
// Scene construction
// ---------------------------------------------------------------------------

/**
 * @brief Builds the scene graph and populates it with game objects.
 *
 * Creates:
 *   - One Eagle (player) at origin, facing forward.
 *   - Two Raptors (enemies) ahead of the player, facing back (rotated 180°).
 *
 * Calls SceneNode::build() on the root, which recursively calls
 * Aircraft::buildCurrent() to register render items with Game.
 */
void World::buildScene()
{
    // Root node — acts as world-space origin.
    mSceneGraph = std::make_unique<SceneNode>(mGame);

    // ---- Player aircraft ------------------------------------------------
    auto player = std::make_unique<Aircraft>(Aircraft::Eagle, mGame);
    mPlayerAircraft = player.get();
    mPlayerAircraft->setPosition(0.0f, 0.0f, 0.0f);
    mPlayerAircraft->setScale(1.0f, 1.0f, 1.0f);
    mSceneGraph->attachChild(std::move(player));

    // ---- Enemy aircraft 1 -----------------------------------------------
    auto enemy1 = std::make_unique<Aircraft>(Aircraft::Raptor, mGame);
    enemy1->setPosition(-5.0f, 0.0f, 10.0f);
    enemy1->setScale(1.0f, 1.0f, 1.0f);
    enemy1->setWorldRotation(0.0f, XM_PI, 0.0f);  // facing player
    mSceneGraph->attachChild(std::move(enemy1));

    // ---- Enemy aircraft 2 -----------------------------------------------
    auto enemy2 = std::make_unique<Aircraft>(Aircraft::Raptor, mGame);
    enemy2->setPosition(5.0f, 0.0f, 10.0f);
    enemy2->setScale(1.0f, 1.0f, 1.0f);
    enemy2->setWorldRotation(0.0f, XM_PI, 0.0f);  // facing player
    mSceneGraph->attachChild(std::move(enemy2));

    // Recursively build render items for all nodes.
    mSceneGraph->build();
}
