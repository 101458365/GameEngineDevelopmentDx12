#pragma once
#include "Entity.hpp"
#include <string>

/**
 * @brief A concrete Entity representing a player or enemy aircraft.
 *
 * Aircraft derives from Entity, meaning it can move (via velocity) and
 * owns a RenderItem that is registered with the Game during buildCurrent().
 *
 * Two types are supported: Eagle (player, blue) and Raptor (enemy, red).
 * The type determines which material is applied to the mesh.
 */
class Aircraft : public Entity
{
public:
    /// @brief Aircraft type, used to select the correct material.
    enum Type
    {
        Eagle,   ///< Player aircraft — blue material.
        Raptor,  ///< Enemy aircraft  — red material.
    };

public:
    /**
     * @brief Constructs an Aircraft of the given type.
     * @param type  Eagle or Raptor.
     * @param game  Back-pointer to the Game (needed for resource access).
     */
    Aircraft(Type type, Game* game);

private:
    /**
     * @brief No per-frame custom draw logic; rendering is via render items.
     */
    virtual void drawCurrent() const override;

    /**
     * @brief Creates the RenderItem and registers it with Game.
     *
     * Called once during Scene::build(). Sets up the mesh (box from
     * "shapeGeo"), selects the correct material (Eagle or Raptor),
     * assigns a CB index, and pushes the RenderItem into Game::mAllRitems.
     * Stores a raw pointer in SceneNode::renderer for later CB updates.
     */
    virtual void buildCurrent() override;

private:
    Type        mType;    ///< Eagle or Raptor.
    std::string mSprite;  ///< Material name ("Eagle" or "Raptor").
};
