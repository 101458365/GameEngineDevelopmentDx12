#pragma once
#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
#include "../../Common/Camera.h"
#include "FrameResource.h"
#include "Command.hpp"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

/**
 * @brief Lightweight structure holding all parameters needed to draw one object.
 *
 * Each Aircraft node owns one RenderItem and keeps a raw pointer (renderer)
 * to it. The GPU constant buffer is updated when NumFramesDirty > 0.
 */
struct RenderItem
{
    RenderItem() = default;

    /// World transform of this object.
    XMFLOAT4X4 World = MathHelper::Identity4x4();

    /// UV transform applied in the vertex shader.
    XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

    /**
     * @brief Number of frame resources still needing a CB update.
     *
     * Set to gNumFrameResources whenever the world matrix changes.
     * Decremented each frame until zero.
     */
    int NumFramesDirty = gNumFrameResources;

    /// Index into the per-object constant buffer on the GPU.
    UINT ObjCBIndex = -1;

    Material*      Mat  = nullptr;
    MeshGeometry*  Geo  = nullptr;

    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    UINT IndexCount         = 0;
    UINT StartIndexLocation = 0;
    int  BaseVertexLocation = 0;
};

class Game;

/**
 * @brief Base node in the scene graph.
 *
 * SceneNode forms the core of the game's entity hierarchy. It manages a
 * transform (position, rotation, scale) and a list of child nodes. The
 * update(), draw(), and build() methods propagate recursively to all children.
 *
 * Subclasses override updateCurrent(), drawCurrent(), and buildCurrent()
 * to implement per-node behaviour.
 *
 * Assignment 2 additions:
 *   - getCategory() returns the node's Category bitmask (default: Category::Scene).
 *   - onCommand() checks the command's category against this node's category
 *     and executes the action if they match, then forwards to all children.
 */
class SceneNode
{
public:
    typedef std::unique_ptr<SceneNode> Ptr;

public:
    /// @brief Constructs a SceneNode with the given Game back-pointer.
    SceneNode(Game* game);

    // -------------------------------------------------------
    // Scene-graph manipulation
    // -------------------------------------------------------
    /// @brief Attaches a child node, taking ownership.
    void attachChild(Ptr child);

    /// @brief Detaches and returns a child node, releasing ownership.
    Ptr  detachChild(const SceneNode& node);

    // -------------------------------------------------------
    // Recursive traversal entry points
    // -------------------------------------------------------
    /// @brief Updates this node and all children.
    void update(const GameTimer& gt);

    /// @brief Draws this node and all children.
    void draw() const;

    /// @brief Builds GPU resources for this node and all children.
    void build();

    // -------------------------------------------------------
    // Command system
    // -------------------------------------------------------

    /**
     * @brief Dispatches a command to this node and all children.
     *
     * If this node's category overlaps the command's category bitmask,
     * the command's action is executed on this node. The command is then
     * forwarded to all children regardless of whether it matched here.
     *
     * @param command  The command to dispatch.
     * @param gt       Game timer passed to the action function.
     */
    void onCommand(const Command& command, const GameTimer& gt);

    /**
     * @brief Returns this node's category bitmask.
     *
     * Override in derived classes to return a specific category.
     * Default returns Category::Scene.
     *
     * @return Category bitmask (one or more Category::Type values OR'd together).
     */
    virtual unsigned int getCategory() const;

    // -------------------------------------------------------
    // Transform accessors
    // -------------------------------------------------------
    XMFLOAT3   getWorldPosition() const;
    void       setPosition(float x, float y, float z);

    XMFLOAT3   getWorldRotation() const;
    void       setWorldRotation(float x, float y, float z);

    XMFLOAT3   getWorldScale() const;
    void       setScale(float x, float y, float z);

    /// @brief Returns the cumulative world-space transform (walks to root).
    XMFLOAT4X4 getWorldTransform() const;

    /// @brief Returns the local transform of this node only.
    XMFLOAT4X4 getTransform() const;

    /// @brief Translates this node by (x, y, z) in local space.
    void       move(float x, float y, float z);

private:
    // -------------------------------------------------------
    // Virtual hooks for subclasses
    // -------------------------------------------------------
    virtual void updateCurrent(const GameTimer& gt);
    void         updateChildren(const GameTimer& gt);

    virtual void drawCurrent() const;
    void         drawChildren() const;

    virtual void buildCurrent();
    void         buildChildren();

protected:
    /// Back-pointer to the Game; used by Aircraft to register render items.
    Game*        game;

    /**
     * @brief Raw pointer to this node's GPU render item.
     *
     * Set in buildCurrent() by subclasses (e.g. Aircraft).
     * Null for non-renderable nodes such as the root or layer nodes.
     */
    RenderItem*  renderer = nullptr;

private:
    XMFLOAT3             mWorldPosition;
    XMFLOAT3             mWorldRotation;
    XMFLOAT3             mWorldScaling;
    std::vector<Ptr>     mChildren;
    SceneNode*           mParent;
};
