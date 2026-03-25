/**
 * @file SceneNode.cpp
 * @brief Implementation of the SceneNode class.
 */

#include "SceneNode.hpp"
#include "Game.hpp"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

/**
 * @brief Initialises transform to identity and clears parent link.
 * @param game  Back-pointer to the owning Game.
 */
SceneNode::SceneNode(Game* game)
    : mChildren()
    , mParent(nullptr)
    , game(game)
    , renderer(nullptr)
{
    mWorldPosition = XMFLOAT3(0.0f, 0.0f, 0.0f);
    mWorldScaling  = XMFLOAT3(1.0f, 1.0f, 1.0f);
    mWorldRotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
}

// ---------------------------------------------------------------------------
// Scene-graph manipulation
// ---------------------------------------------------------------------------

/**
 * @brief Attaches a child node and takes ownership.
 * @param child  The node to attach; its parent pointer is set to this.
 */
void SceneNode::attachChild(Ptr child)
{
    child->mParent = this;
    mChildren.push_back(std::move(child));
}

/**
 * @brief Finds, detaches, and returns a child node.
 * @param node  Reference to the node to detach.
 * @return Owning pointer to the detached node.
 */
SceneNode::Ptr SceneNode::detachChild(const SceneNode& node)
{
    auto found = std::find_if(mChildren.begin(), mChildren.end(),
        [&](Ptr& p) { return p.get() == &node; });
    assert(found != mChildren.end());

    Ptr result      = std::move(*found);
    result->mParent = nullptr;
    mChildren.erase(found);
    return result;
}

// ---------------------------------------------------------------------------
// Recursive traversal
// ---------------------------------------------------------------------------

/**
 * @brief Updates this node then recursively updates children.
 * @param gt  Game timer.
 */
void SceneNode::update(const GameTimer& gt)
{
    updateCurrent(gt);
    updateChildren(gt);
}

/**
 * @brief Default update hook — does nothing for non-Entity nodes.
 *
 * Overridden by Entity to apply velocity and mark the CB dirty.
 */
void SceneNode::updateCurrent(const GameTimer& gt)
{
    // Base SceneNode has no per-frame update logic.
}

/** @brief Propagates update to all children. */
void SceneNode::updateChildren(const GameTimer& gt)
{
    for (Ptr& child : mChildren)
        child->update(gt);
}

/** @brief Draws this node then recursively draws children. */
void SceneNode::draw() const
{
    drawCurrent();
    drawChildren();
}

/**
 * @brief Default draw hook — no-op for non-renderable nodes.
 *
 * Actual rendering is done via the render-item list in Game::DrawRenderItems().
 */
void SceneNode::drawCurrent() const
{
    // Rendering is handled by Game::DrawRenderItems via the render-item list.
}

/** @brief Propagates draw to all children. */
void SceneNode::drawChildren() const
{
    for (const Ptr& child : mChildren)
        child->draw();
}

/** @brief Builds this node then recursively builds children. */
void SceneNode::build()
{
    buildCurrent();
    buildChildren();
}

/**
 * @brief Default build hook — no-op for non-renderable nodes.
 *
 * Overridden by Aircraft to create and register a RenderItem.
 */
void SceneNode::buildCurrent()
{
    // Non-renderable nodes (e.g. root, layer nodes) have nothing to build.
}

/** @brief Propagates build to all children. */
void SceneNode::buildChildren()
{
    for (const Ptr& child : mChildren)
        child->build();
}

// ---------------------------------------------------------------------------
// Command system
// ---------------------------------------------------------------------------

/**
 * @brief Returns Category::Scene by default.
 *
 * Derived classes (e.g. Aircraft) override this to return their specific
 * category so commands are routed to the correct nodes.
 */
unsigned int SceneNode::getCategory() const
{
    return Category::Scene;
}

/**
 * @brief Executes the command if this node matches, then forwards to children.
 *
 * The bitwise AND checks whether any bit in the command's category overlaps
 * with this node's category. If so, the action is called on this node.
 * The command is always forwarded to children regardless of match.
 *
 * @param command  The command to dispatch.
 * @param gt       Game timer passed through to the action.
 */
void SceneNode::onCommand(const Command& command, const GameTimer& gt)
{
    if (command.category & getCategory())
        command.action(*this, gt);

    for (Ptr& child : mChildren)
        child->onCommand(command, gt);
}

// ---------------------------------------------------------------------------
// Transform accessors
// ---------------------------------------------------------------------------

/** @brief Returns this node's local position. */
XMFLOAT3 SceneNode::getWorldPosition() const
{
    return mWorldPosition;
}

/**
 * @brief Sets the local position.
 * @param x, y, z  Position components.
 */
void SceneNode::setPosition(float x, float y, float z)
{
    mWorldPosition = XMFLOAT3(x, y, z);
}

/** @brief Returns this node's local Euler rotation (radians). */
XMFLOAT3 SceneNode::getWorldRotation() const
{
    return mWorldRotation;
}

/**
 * @brief Sets the local Euler rotation (radians).
 * @param x, y, z  Pitch, yaw, roll in radians.
 */
void SceneNode::setWorldRotation(float x, float y, float z)
{
    mWorldRotation = XMFLOAT3(x, y, z);
}

/** @brief Returns this node's local scale. */
XMFLOAT3 SceneNode::getWorldScale() const
{
    return mWorldScaling;
}

/**
 * @brief Sets the local scale.
 * @param x, y, z  Scale factors.
 */
void SceneNode::setScale(float x, float y, float z)
{
    mWorldScaling = XMFLOAT3(x, y, z);
}

/**
 * @brief Computes the accumulated world-space transform by walking to the root.
 *
 * Each node's local transform is composed left-to-right from the root down,
 * so a child's world matrix = parent_world * child_local.
 *
 * @return The world-space 4x4 matrix for this node.
 */
XMFLOAT4X4 SceneNode::getWorldTransform() const
{
    XMFLOAT4X4 transform = MathHelper::Identity4x4();
    XMMATRIX   T         = XMLoadFloat4x4(&transform);

    for (const SceneNode* node = this; node != nullptr; node = node->mParent)
    {
        auto nodeLocal = node->getTransform();
        XMMATRIX Tp = XMLoadFloat4x4(&nodeLocal);
        T = Tp * T;   // accumulate from root downward
    }

    XMStoreFloat4x4(&transform, T);
    return transform;
}

/**
 * @brief Computes only this node's local transform (S * Rx * Ry * Rz * T).
 * @return The local 4x4 matrix.
 */
XMFLOAT4X4 SceneNode::getTransform() const
{
    XMFLOAT4X4 transform;
    XMStoreFloat4x4(&transform,
        XMMatrixScaling(mWorldScaling.x, mWorldScaling.y, mWorldScaling.z) *
        XMMatrixRotationX(mWorldRotation.x) *
        XMMatrixRotationY(mWorldRotation.y) *
        XMMatrixRotationZ(mWorldRotation.z) *
        XMMatrixTranslation(mWorldPosition.x, mWorldPosition.y, mWorldPosition.z));
    return transform;
}

/**
 * @brief Translates this node's local position by (x, y, z).
 * @param x, y, z  Delta movement in local space.
 */
void SceneNode::move(float x, float y, float z)
{
    mWorldPosition.x += x;
    mWorldPosition.y += y;
    mWorldPosition.z += z;
}
