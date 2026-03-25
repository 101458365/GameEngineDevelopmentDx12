/**
 * @file World.cpp
 * @brief Implementation of the World class.
 *
 * World builds and manages the scene graph. It creates the player Eagle and
 * two enemy Raptors, then delegates update/draw to the root SceneNode each frame.
 *
 * Assignment 2 changes:
 *   - update() resets the player velocity, drains the CommandQueue into
 *     the scene graph via onCommand(), then runs the regular update step.
 *   - handlePlayerInput() is removed — Player handles input now.
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
 *
 * Steps:
 *   1. Reset player velocity to zero so the aircraft stops when no key is held.
 *   2. Drain the CommandQueue — each command is dispatched through the scene
 *      graph via onCommand(), which routes it to matching nodes.
 *   3. Run the regular scene-graph update (Entity moves by velocity * dt).
 *
 * @param gt  Game timer.
 */
void World::update(const GameTimer& gt)
{
    // Reset player velocity each frame — commands will re-add it if keys are held.
    if (mPlayerAircraft)
        mPlayerAircraft->setVelocity(0.0f, 0.0f, 0.0f);

    // Drain the command queue and dispatch each command through the scene graph.
    while (!mCommandQueue.isEmpty())
        mSceneGraph->onCommand(mCommandQueue.pop(), gt);

    // Regular scene-graph update (applies velocities, updates CBs, etc.)
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
// Scene construction
// ---------------------------------------------------------------------------

/**
 * @brief Builds the scene graph and populates it with game objects.
 *
 * Creates:
 *   - One Eagle (player) at origin, facing forward.
 *   - Two Raptors (enemies) ahead of the player, facing back (rotated 180 degrees).
 *   - A floor grid below the aircraft.
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
    mPlayerAircraft->setScale(2.0f, 0.05f, 2.0f);
    mPlayerAircraft->setMaterial("bricks0");
    mSceneGraph->attachChild(std::move(player));

    // ---- Enemy aircraft 1 -----------------------------------------------
    auto enemy1 = std::make_unique<Aircraft>(Aircraft::Raptor, mGame);
    enemy1->setPosition(-5.0f, 1.0f, 10.0f);
    enemy1->setScale(2.0f, 0.1f, 2.0f);
    enemy1->setWorldRotation(0.0f, XM_PI, 0.0f);  // facing player
    enemy1->setMaterial("mirror0");
    mPlayerAircraft->attachChild(std::move(enemy1));

    // ---- Enemy aircraft 2 -----------------------------------------------
    auto enemy2 = std::make_unique<Aircraft>(Aircraft::Raptor, mGame);
    enemy2->setPosition(5.0f, 1.0f, 10.0f);
    enemy2->setScale(2.0f, 0.1f, 2.0f);
    enemy2->setWorldRotation(0.0f, XM_PI, 0.0f);  // facing player
    enemy2->setMaterial("mirror0");
    mPlayerAircraft->attachChild(std::move(enemy2));

    // ---- Floor grid ----
    auto floor = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&floor->World, XMMatrixScaling(5.0f, 1.0f, 5.0f) * XMMatrixTranslation(0.0f, -1.0f, 0.0f));
    XMStoreFloat4x4(&floor->TexTransform, XMMatrixScaling(8.0f, 8.0f, 1.0f));
    floor->ObjCBIndex         = (UINT)mGame->getRenderItems().size();
    floor->Mat                = mGame->getMaterials()["tile0"].get();
    floor->Geo                = mGame->getGeometries()["shapeGeo"].get();
    floor->PrimitiveType      = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    floor->IndexCount         = floor->Geo->DrawArgs["grid"].IndexCount;
    floor->StartIndexLocation = floor->Geo->DrawArgs["grid"].StartIndexLocation;
    floor->BaseVertexLocation = floor->Geo->DrawArgs["grid"].BaseVertexLocation;
    mGame->getRenderItems().push_back(std::move(floor));

    // Recursively build render items for all nodes.
    mSceneGraph->build();
}
