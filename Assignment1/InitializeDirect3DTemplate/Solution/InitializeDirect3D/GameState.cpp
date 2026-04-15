/**
 * @file GameState.cpp
 * @brief Implementation of the GameState class.
 */

#include "GameState.hpp"
#include "StateStack.hpp"
#include "Game.hpp"

 /**
  * @brief Constructs the GameState, building the 3D scene.
  *
  * World::buildScene() creates the Eagle, two Raptors, and the floor grid
  * and registers their render items with Game. Player sets up default
  * arrow-key bindings mapped to AircraftMover commands.
  *
  * @param stack    The owning StateStack.
  * @param context  Shared application context.
  */
GameState::GameState(StateStack& stack, Context context)
    : State(stack, context)
    , mWorld(context.game)
    , mPlayer()
{
    // Build the scene into this World instance (aircraft + floor render items).
    mWorld.buildScene();

    // Frame resources were sized for the sky item only. Now that buildScene()
    // has pushed additional render items, rebuild them at the correct size.
    context.game->RebuildFrameResources();

    // Populate the opaque list so Draw() renders 3D geometry.
    for (auto& e : context.game->getRenderItems())
        if (e->Mat != context.game->getMaterials()["sky"].get())
            context.game->getOpaqueRitems().push_back(e.get());

    SetWindowTextA(context.game->MainWnd(),
        "Aircraft Shooter  |  WASD: Camera   Arrows: Move   Q/E: Up/Down   ESC: Pause");
}

GameState::~GameState()
{
    // Clear the opaque list so Draw() stops rendering 3D geometry
    // when we return to the Title or Menu screens.
    getContext().game->getOpaqueRitems().clear();

    // Remove the scene render items (aircraft + floor) that buildScene() added,
    // keeping only the sky sphere (index 0) so re-entering GameState doesn't
    // accumulate duplicate items.
    auto& allItems = getContext().game->getRenderItems();
    auto& skyItems = getContext().game->getSkyRitems();
    if (allItems.size() > 1)
        allItems.erase(allItems.begin() + 1, allItems.end());

    // Re-point skyItems[0] in case the vector was reallocated.
    skyItems.clear();
    if (!allItems.empty())
        skyItems.push_back(allItems[0].get());
}

// ---------------------------------------------------------------------------
// State interface
// ---------------------------------------------------------------------------

/**
 * @brief Delegates draw to the World scene graph.
 *
 * Actual GPU draw calls are issued by Game::DrawRenderItems() using the
 * render-item list that was populated during buildScene(). This call
 * propagates through the scene graph for any future per-node draw logic.
 */
void GameState::draw()
{
    mWorld.draw();
}

/**
 * @brief Updates the World and processes player input each frame.
 *
 * Pushes movement commands from Player into the World's CommandQueue,
 * which World drains and dispatches through the scene graph before
 * running the regular velocity-based update step.
 *
 * @param gt  Game timer providing delta time.
 * @return    False — blocks states below from updating.
 */
bool GameState::update(const GameTimer& gt)
{
    mWorld.update(gt);

    CommandQueue& commands = mWorld.getCommandQueue();
    mPlayer.handleRealtimeInput(commands, gt);

    return false;
}

/**
 * @brief Handles one-time key events.
 *
 * Escape pushes PauseState on top of GameState. The game scene remains
 * on the stack and visible behind the pause overlay, but its update()
 * is blocked because PauseState::update() returns false.
 *
 * @param key  Virtual-key code of the pressed key.
 * @return     False — consumes the key event.
 */
bool GameState::handleEvent(WPARAM key)
{
    if (key == VK_ESCAPE)
    {
        requestStackPush(States::Pause);
        return false;
    }

    // Forward other key events to Player for discrete actions (e.g. fire).
    CommandQueue& commands = mWorld.getCommandQueue();
    mPlayer.handleEvent(commands, GameTimer());

    return false;
}