/**
 * @file Aircraft.cpp
 * @brief Implementation of the Aircraft class.
 */

#include "Aircraft.hpp"
#include "Game.hpp"

/**
 * @brief Constructs an Aircraft, setting the material name from its type.
 *
 * The material name ("Eagle" or "Raptor") is used during buildCurrent()
 * to look up the correct Material in the Game's material map.
 *
 * @param type  Eagle (player) or Raptor (enemy).
 * @param game  Back-pointer to the owning Game.
 */
Aircraft::Aircraft(Type type, Game* game)
    : Entity(game)
    , mType(type)
{
    switch (type)
    {
    case Eagle:  mSprite = "Eagle";  break;
    case Raptor: mSprite = "Raptor"; break;
    default:     mSprite = "Eagle";  break;
    }
}

// ---------------------------------------------------------------------------
// SceneNode virtual overrides
// ---------------------------------------------------------------------------

/**
 * @brief Draw hook — rendering is handled by Game::DrawRenderItems().
 *
 * Aircraft does not issue draw calls directly; its RenderItem was registered
 * in buildCurrent() and Game draws it via the opaque render-item list.
 */
void Aircraft::drawCurrent() const
{
    // Intentionally empty — Game::DrawRenderItems() handles rendering.
}

/**
 * @brief Creates and registers a RenderItem with the Game.
 *
 * Allocates a RenderItem, sets its world matrix, material, geometry ("shapeGeo"
 * → "box" sub-mesh), and CB index, then pushes it into Game::mAllRitems.
 * A raw non-owning pointer is stored in SceneNode::renderer so that
 * Entity::updateCurrent() can update the world matrix and dirty flag each frame.
 *
 * @note "shapeGeo" must already exist in Game::mGeometries before buildScene()
 *       is called. This is guaranteed because Game::BuildShapeGeometry() runs
 *       before Game::BuildRenderItems() → World::buildScene().
 */
void Aircraft::buildCurrent()
{
    auto render = std::make_unique<RenderItem>();
    renderer = render.get();

    // Initial world transform from current node position.
    renderer->World = getTransform();

    // Assign the next available CB index.
    renderer->ObjCBIndex = (UINT)game->getRenderItems().size();

    // Select the material registered in Game::BuildMaterials().
    renderer->Mat = game->getMaterials()[mSprite].get();

    // Use the shared shape geometry; aircraft are represented as boxes.
    renderer->Geo           = game->getGeometries()["shapeGeo"].get();
    renderer->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    renderer->IndexCount          = renderer->Geo->DrawArgs["box"].IndexCount;
    renderer->StartIndexLocation  = renderer->Geo->DrawArgs["box"].StartIndexLocation;
    renderer->BaseVertexLocation  = renderer->Geo->DrawArgs["box"].BaseVertexLocation;

    // Hand ownership to Game; keep a raw pointer for per-frame CB updates.
    game->getRenderItems().push_back(std::move(render));
}
