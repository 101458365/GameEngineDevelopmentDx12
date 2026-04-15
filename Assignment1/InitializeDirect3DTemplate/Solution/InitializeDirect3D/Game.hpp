#pragma once
#include "StateStack.hpp"
#include "TitleState.hpp"
#include "MenuState.hpp"
#include "GameState.hpp"
#include "PauseState.hpp"
#include "World.hpp"
#include "Player.hpp"

/**
 * @brief Main application class that manages DirectX 12 initialization,
 *        rendering pipeline, frame resources, and the game loop.
 *
 * Game inherits from D3DApp and owns all low-level GPU resources:
 * textures, shaders, PSOs, descriptor heaps, constant buffers, and
 * frame resources.
 *
 * Game Engine Project additions:
 *   - Owns a StateStack instead of World and Player directly.
 *   - Registers TitleState, MenuState, GameState, and PauseState.
 *   - Pushes TitleState on startup so the flow is:
 *       Title → Menu → Game ↔ Pause
 *   - Update() delegates to StateStack::update().
 *   - Draw() delegates to StateStack::draw() for state-specific rendering.
 *   - OnKeyboardInput() handles camera (WASD) and passes key events to
 *     StateStack::handleEvent() for state transitions.
 *   - Application exits when the StateStack becomes empty (Exit selected).
 */
class Game : public D3DApp
{
public:
    /// @brief Constructs the Game with a Windows instance handle.
    Game(HINSTANCE hInstance);
    Game(const Game& rhs) = delete;
    Game& operator=(const Game& rhs) = delete;
    ~Game();

    virtual bool Initialize() override;
    void BuildFrameResources();
    void BuildOpaqueRenderItems();

    /// @brief Flushes GPU, clears and rebuilds frame resources at the new item count.
    /// Called by GameState after buildScene() to correctly size per-object CBs.
    void RebuildFrameResources();

private:
    // -------------------------------------------------------
    // D3DApp overrides
    // -------------------------------------------------------
    virtual void OnResize()    override;
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt)   override;

    virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

    // -------------------------------------------------------
    // Per-frame update helpers
    // -------------------------------------------------------

    /// @brief Handles camera-only keyboard input (WASD).
    void OnKeyboardInput(const GameTimer& gt);

    void AnimateMaterials(const GameTimer& gt);
    void UpdateObjectCBs(const GameTimer& gt);
    void UpdateMaterialCBs(const GameTimer& gt);
    void UpdateMainPassCB(const GameTimer& gt);

    // -------------------------------------------------------
    // One-time initialization helpers
    // -------------------------------------------------------
    /// @brief Loads all DDS textures from disk into GPU memory.
    void LoadTextures();
    void BuildRootSignature();
    void BuildDescriptorHeaps();
    void BuildShadersAndInputLayout();
    void BuildShapeGeometry();
    void BuildPSOs();
    void BuildMaterials();
    void BuildRenderItems();

    /// @brief Registers all states with the StateStack.
    void RegisterStates();

    /// @brief Draws GDI text overlay for Title / Menu states.
    void DrawOverlayText();

    /// @brief Issues draw calls for a list of render items.
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList,
        const std::vector<RenderItem*>& ritems);

    std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

private:
    // -------------------------------------------------------
    // Frame resources
    // -------------------------------------------------------
    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int            mCurrFrameResourceIndex = 0;

    UINT mCbvSrvDescriptorSize = 0;

    // -------------------------------------------------------
    // GPU pipeline objects
    // -------------------------------------------------------
    ComPtr<ID3D12RootSignature>  mRootSignature = nullptr;
    ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
    std::unordered_map<std::string, std::unique_ptr<Material>>     mMaterials;
    std::unordered_map<std::string, std::unique_ptr<Texture>>      mTextures;
    std::unordered_map<std::string, ComPtr<ID3DBlob>>              mShaders;
    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>>   mPSOs;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

    // -------------------------------------------------------
    // Render item lists
    // -------------------------------------------------------
    /// @brief All render items (owns memory).
    std::vector<std::unique_ptr<RenderItem>> mAllRitems;

    /// @brief Opaque render items (non-owning view).
    std::vector<RenderItem*> mOpaqueRitems;

    /// @brief Sky render items (non-owning view).
    std::vector<RenderItem*> mSkyRitems;

    UINT mSkyTexHeapIndex = 0;

    // -------------------------------------------------------
    // Per-pass constant buffer
    // -------------------------------------------------------
    PassConstants mMainPassCB;

    // -------------------------------------------------------
    // Camera & input
    // -------------------------------------------------------
    POINT  mLastMousePos;
    Camera mCamera;

    // -------------------------------------------------------
    // State stack — replaces direct World + Player ownership
    // -------------------------------------------------------

    /// @brief The state stack managing Title / Menu / Game / Pause states.
    StateStack mStateStack;

public:
    // -------------------------------------------------------
    // Accessors used by SceneNode / Aircraft during build()
    // -------------------------------------------------------
    /// @brief Returns the list that Aircraft::buildCurrent() pushes into.
    std::vector<std::unique_ptr<RenderItem>>&
        getRenderItems() { return mAllRitems; }

    /// @brief Returns the material map so Aircraft can look up its material.
    std::unordered_map<std::string, std::unique_ptr<Material>>&
        getMaterials() { return mMaterials; }

    /// @brief Returns the geometry map so Aircraft can look up "shapeGeo".
    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>>&
        getGeometries() { return mGeometries; }

    /// @brief Returns the render-item lists so GameState can populate them.
    std::vector<RenderItem*>& getOpaqueRitems() { return mOpaqueRitems; }
    std::vector<RenderItem*>& getSkyRitems() { return mSkyRitems; }
};