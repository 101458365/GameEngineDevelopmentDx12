#pragma once
#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
#include "../../Common/Camera.h"
#include "FrameResource.h"

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
