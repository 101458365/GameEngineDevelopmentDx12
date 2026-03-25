#pragma once
#include "World.hpp"
#include "Player.hpp"

/**
 * @brief Main application class that manages DirectX 12 initialization,
 *        rendering pipeline, frame resources, and the game loop.
 *
 * Game inherits from D3DApp and owns all low-level GPU resources:
 * textures, shaders, PSOs, descriptor heaps, constant buffers, and
 * frame resources. It delegates game-logic to World and the scene graph.
 *
 * Assignment 2 additions:
 *   - Owns a Player instance (mPlayer) that handles all input.
 *   - processInput() is called each frame from Update(), replacing the
 *     old direct GetAsyncKeyState calls for aircraft movement.
 *   - OnKeyboardInput() now only handles camera movement (WASD).
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

private:
    // -------------------------------------------------------
    // D3DApp overrides
    // -------------------------------------------------------
    virtual void OnResize()    override;
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt)   override;

    virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
    virtual void OnMouseUp  (WPARAM btnState, int x, int y) override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

    // -------------------------------------------------------
    // Per-frame update helpers
    // -------------------------------------------------------

    /// @brief Delegates to Player::handleEvent and Player::handleRealtimeInput.
    void processInput();

    /// @brief Handles camera-only keyboard input (WASD).
    void OnKeyboardInput(const GameTimer& gt);

    void AnimateMaterials (const GameTimer& gt);
    void UpdateObjectCBs  (const GameTimer& gt);
    void UpdateMaterialCBs(const GameTimer& gt);
    void UpdateMainPassCB (const GameTimer& gt);

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
    void BuildFrameResources();
    void BuildMaterials();
    void BuildRenderItems();

    /// @brief Issues draw calls for a list of render items.
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList,
                         const std::vector<RenderItem*>& ritems);

    std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

private:
    // -------------------------------------------------------
    // Frame resources
    // -------------------------------------------------------
    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource      = nullptr;
    int            mCurrFrameResourceIndex = 0;

    UINT mCbvSrvDescriptorSize = 0;

    // -------------------------------------------------------
    // GPU pipeline objects
    // -------------------------------------------------------
    ComPtr<ID3D12RootSignature>  mRootSignature    = nullptr;
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

    /// Stored timer pointer so processInput() can pass it to Player.
    const GameTimer* mCurrentGt = nullptr;

    // -------------------------------------------------------
    // Game world & player
    // -------------------------------------------------------
    World  mWorld;

    /// @brief Handles all player input and key-binding management.
    Player mPlayer;

public:
    // -------------------------------------------------------
    // Accessors used by SceneNode / Aircraft during build()
    // -------------------------------------------------------
    /// @brief Returns the list that Aircraft::buildCurrent() pushes into.
    std::vector<std::unique_ptr<RenderItem>>&
        getRenderItems() { return mAllRitems; }

    /// @brief Returns the material map so Aircraft can look up its material.
    std::unordered_map<std::string, std::unique_ptr<Material>>&
        getMaterials()   { return mMaterials; }

    /// @brief Returns the geometry map so Aircraft can look up "shapeGeo".
    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>>&
        getGeometries()  { return mGeometries; }
};
