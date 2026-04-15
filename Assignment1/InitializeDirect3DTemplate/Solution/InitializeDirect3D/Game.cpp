/**
 * @file Game.cpp
 * @brief Implementation of the Game class.
 *
 * Game is the top-level DirectX 12 application. It owns all GPU resources
 * (textures, shaders, PSOs, frame resources, constant buffers).
 *
 * Game Engine Project changes:
 *   - Owns a StateStack instead of World and Player directly.
 *   - Registers TitleState, MenuState, GameState, PauseState on startup.
 *   - TitleState is pushed first — flow is Title → Menu → Game ↔ Pause.
 *   - Update() delegates to StateStack::update().
 *   - Draw() calls StateStack::draw() then issues GPU draw calls.
 *   - OnKeyboardInput() handles camera (WASD).
 *   - Key-down events from D3DApp::MsgProc are forwarded to
 *     StateStack::handleEvent() for state transitions.
 *   - App exits when StateStack becomes empty.
 *
 * Keyboard controls:
 *   W / S / A / D  – fly the camera (all states)
 *   Arrow keys      – move the player aircraft (GameState only)
 *   Q / E           – move aircraft up / down (GameState only)
 *   ESC             – pause game / resume
 *   BACKSPACE       – return to main menu from pause
 *   UP / DOWN       – navigate menu options
 *   ENTER           – confirm menu selection
 */

#include "Game.hpp"

const int gNumFrameResources = 3;

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

/**
 * @brief Constructs the Game, initialising the StateStack with a Context.
 * @param hInstance  Windows application instance handle.
 */
Game::Game(HINSTANCE hInstance)
    : D3DApp(hInstance)
    , mStateStack(Context(this))
{
}

Game::~Game()
{
    if (md3dDevice != nullptr)
        FlushCommandQueue();
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

/**
 * @brief Initialises DirectX 12 resources and pushes the TitleState.
 *
 * Called once at startup. Sets up the camera, loads textures, builds the
 * pipeline state, geometry, and materials. Registers all states with the
 * StateStack and pushes TitleState as the initial state.
 *
 * Note: BuildRenderItems() is intentionally left empty here — GameState
 * calls World::buildScene() when it is constructed, which pushes render
 * items into Game::mAllRitems at that point.
 */
bool Game::Initialize()
{
    if (!D3DApp::Initialize())
        return false;

    mCamera.SetPosition(0.0f, 10.0f, -20.0f);
    mCamera.Pitch(XMConvertToRadians(20.0f));

    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    mCbvSrvDescriptorSize =
        md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    LoadTextures();
    BuildRootSignature();
    BuildDescriptorHeaps();
    BuildShadersAndInputLayout();
    BuildShapeGeometry();
    RegisterStates();
    mStateStack.pushState(States::Title);
    BuildMaterials();
    //BuildRenderItems();       // sky sphere only (index 0)
    BuildPSOs();
    BuildFrameResources();    // sized for sky item only; GameState will call
    // RebuildFrameResources() after buildScene() adds more

    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    FlushCommandQueue();

    return true;
}

// ---------------------------------------------------------------------------
// State registration
// ---------------------------------------------------------------------------

/**
 * @brief Registers all state classes with the StateStack factory.
 *
 * Each state is associated with a States::ID. When the stack needs to
 * create a state it calls the registered lambda, which news the concrete
 * type without the stack needing to know about it.
 */
void Game::RegisterStates()
{
    mStateStack.registerState<TitleState>(States::Title);
    mStateStack.registerState<MenuState>(States::Menu);
    mStateStack.registerState<GameState>(States::Game);
    mStateStack.registerState<PauseState>(States::Pause);
}

// ---------------------------------------------------------------------------
// D3DApp overrides
// ---------------------------------------------------------------------------

/** @brief Updates projection matrix when the window is resized. */
void Game::OnResize()
{
    D3DApp::OnResize();
    mCamera.SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
}

/**
 * @brief Per-frame update: camera input, state stack, and GPU uploads.
 *
 * If the StateStack becomes empty (Exit was selected from the menu),
 * PostQuitMessage is called to close the application.
 *
 * @param gt  Game timer providing delta time and total time.
 */
void Game::Update(const GameTimer& gt)
{
    OnKeyboardInput(gt);

    // Update all active states via the stack.
    mStateStack.update(gt);

    // Exit the application if the stack is empty (user chose Exit).
    if (mStateStack.isEmpty())
        PostQuitMessage(0);

    // Advance circular frame-resource index.
    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    // Wait if GPU has not finished with this frame resource yet.
    if (mCurrFrameResource->Fence != 0 &&
        mFence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

    AnimateMaterials(gt);
    UpdateObjectCBs(gt);
    UpdateMaterialCBs(gt);
    UpdateMainPassCB(gt);
}

/**
 * @brief Records and submits the draw command list for one frame.
 *
 * Clears the screen with a colour appropriate to the current state,
 * then calls StateStack::draw() to let active states submit geometry,
 * then issues the GPU draw calls for all registered render items.
 *
 * @param gt  Game timer (unused directly here).
 */
void Game::Draw(const GameTimer& gt)
{
    //mStateStack.draw();
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;
    ThrowIfFailed(cmdListAlloc->Reset());
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    auto t1 = CD3DX12_RESOURCE_BARRIER::Transition(
        CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    mCommandList->ResourceBarrier(1, &t1);

    // Clear colour changes per state for visual distinction.
    // Title = dark navy, Menu = dark teal, Game/Pause = sky blue.
    FLOAT clearColor[4];
    if (!mOpaqueRitems.empty())
    {
        // GameState or PauseState is active — sky blue.
        /*clearColor[0] = 0.53f; clearColor[1] = 0.81f;
        clearColor[2] = 0.98f; clearColor[3] = 1.0f;*/

    }
    else if (mStateStack.isEmpty())
    {
        clearColor[0] = 0.0f; clearColor[1] = 0.0f;
        clearColor[2] = 0.0f; clearColor[3] = 1.0f;
    }
    else
    {
        // Title or Menu — dark navy.
        clearColor[0] = 0.05f; clearColor[1] = 0.05f;
        clearColor[2] = 0.15f; clearColor[3] = 1.0f;
    }

    mCommandList->ClearRenderTargetView(
        CurrentBackBufferView(), clearColor, 0, nullptr);
    mCommandList->ClearDepthStencilView(
        DepthStencilView(),
        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
        1.0f, 0, 0, nullptr);

    auto bbv = CurrentBackBufferView();
    auto dsv = DepthStencilView();
    mCommandList->OMSetRenderTargets(1, &bbv, true, &dsv);

    BuildOpaqueRenderItems();
    OutputDebugStringA(("Opaque count: " + std::to_string(mOpaqueRitems.size()) + "\n").c_str());

    // Only bind GPU resources and draw geometry when there are render items.
    if (!mOpaqueRitems.empty())
    {
        ID3D12DescriptorHeap* heaps[] = { mSrvDescriptorHeap.Get() };
        mCommandList->SetDescriptorHeaps(_countof(heaps), heaps);
        mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

        auto passCB = mCurrFrameResource->PassCB->Resource();
        mCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

        auto matBuffer = mCurrFrameResource->MaterialBuffer->Resource();
        mCommandList->SetGraphicsRootShaderResourceView(2, matBuffer->GetGPUVirtualAddress());

        CD3DX12_GPU_DESCRIPTOR_HANDLE skyTex(
            mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
        skyTex.Offset(mSkyTexHeapIndex, mCbvSrvDescriptorSize);
        mCommandList->SetGraphicsRootDescriptorTable(3, skyTex);

        mCommandList->SetGraphicsRootDescriptorTable(
            4, mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

        // Let states draw (GameState delegates to World::draw()).
        mStateStack.draw();

        DrawRenderItems(mCommandList.Get(), mOpaqueRitems);

        mCommandList->SetPipelineState(mPSOs["sky"].Get());
        DrawRenderItems(mCommandList.Get(), mSkyRitems);
    }
    else
    {
        // No render items yet (Title / Menu) — just let states draw.
        mStateStack.draw();
    }

    auto t2 = CD3DX12_RESOURCE_BARRIER::Transition(
        CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    mCommandList->ResourceBarrier(1, &t2);

    ThrowIfFailed(mCommandList->Close());

    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    ThrowIfFailed(mSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    mCurrFrameResource->Fence = ++mCurrentFence;
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);

    // Draw GDI text overlay for non-game states (Title / Menu / Pause).
    // This runs after Present so DX12 and GDI don't conflict.
    if (mOpaqueRitems.empty())
        DrawOverlayText();
}

void Game::BuildOpaqueRenderItems()
{
    mOpaqueRitems.clear();

    for (auto& e : mAllRitems) // or getRenderItems()
    {
        // Skip sky
        if (e->Mat != mMaterials["sky"].get())
        {
            mOpaqueRitems.push_back(e.get());
        }
    }
}

// ---------------------------------------------------------------------------
// GDI text overlay (Title / Menu / Pause)
// ---------------------------------------------------------------------------

/**
 * @brief Draws state-specific text onto the window using GDI.
 *
 * Called after Present() on frames where the opaque render list is empty
 * (i.e. Title and Menu states). GDI renders directly onto the HWND after
 * the swap chain has presented, so there is no conflict with DX12.
 *
 * This gives visible on-screen feedback without needing a sprite renderer
 * or ImGui integration.
 */
void Game::DrawOverlayText()
{
    HDC hdc = GetDC(mhMainWnd);
    if (!hdc) return;

    RECT rc;
    GetClientRect(mhMainWnd, &rc);

    // Transparent text background so the DX12 clear colour shows through.
    SetBkMode(hdc, TRANSPARENT);

    // Title state — one big centred prompt.
    // Menu state  — show both options, highlight the selected one.
    // We distinguish by checking the window title string.
    char title[512] = {};
    GetWindowTextA(mhMainWnd, title, sizeof(title));

    const bool isTitle = (strstr(title, "Press Any Key") != nullptr);
    const bool isMenu = (strstr(title, "Main Menu") != nullptr);

    HFONT hFontLarge = CreateFontA(
        72, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
    HFONT hFontSmall = CreateFontA(
        36, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");

    HFONT hOldFont = (HFONT)SelectObject(hdc, hFontLarge);

    if (isTitle)
    {
        SetTextColor(hdc, RGB(220, 220, 255));
        RECT r = rc;
        r.top = rc.bottom / 3;
        DrawTextA(hdc, "AIRCRAFT SHOOTER", -1, &r, DT_CENTER | DT_SINGLELINE);

        SelectObject(hdc, hFontSmall);
        SetTextColor(hdc, RGB(180, 180, 220));
        r.top = rc.bottom / 2;
        DrawTextA(hdc, "Press any key to start", -1, &r, DT_CENTER | DT_SINGLELINE);
    }
    else if (isMenu)
    {
        SetTextColor(hdc, RGB(220, 220, 255));
        RECT r = rc;
        r.top = rc.bottom / 4;
        DrawTextA(hdc, "MAIN MENU", -1, &r, DT_CENTER | DT_SINGLELINE);

        SelectObject(hdc, hFontSmall);

        const bool playSelected = (strstr(title, "[> Play <]") != nullptr);

        // Play option
        SetTextColor(hdc, playSelected ? RGB(255, 255, 100) : RGB(160, 160, 200));
        r.top = rc.bottom / 2 - 30;
        DrawTextA(hdc, playSelected ? "> Play <" : "Play", -1, &r, DT_CENTER | DT_SINGLELINE);

        // Exit option
        SetTextColor(hdc, !playSelected ? RGB(255, 255, 100) : RGB(160, 160, 200));
        r.top = rc.bottom / 2 + 30;
        DrawTextA(hdc, !playSelected ? "> Exit <" : "Exit", -1, &r, DT_CENTER | DT_SINGLELINE);

        SetTextColor(hdc, RGB(120, 120, 160));
        SelectObject(hdc, hFontSmall);
        r.top = rc.bottom * 3 / 4;
        DrawTextA(hdc, "UP / DOWN to navigate   ENTER to select", -1, &r, DT_CENTER | DT_SINGLELINE);
    }

    SelectObject(hdc, hOldFont);
    DeleteObject(hFontLarge);
    DeleteObject(hFontSmall);
    ReleaseDC(mhMainWnd, hdc);
}

// ---------------------------------------------------------------------------
// Mouse input
// ---------------------------------------------------------------------------

/** @brief Records the mouse position on button-down. */
void Game::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;
    SetCapture(mhMainWnd);
}

/** @brief Releases mouse capture on button-up. */
void Game::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

/**
 * @brief Rotates the camera while the left mouse button is held.
 * @param btnState  Bitmask of pressed buttons.
 * @param x, y      Current cursor position.
 */
void Game::OnMouseMove(WPARAM btnState, int x, int y)
{
    if ((btnState & MK_LBUTTON) != 0)
    {
        float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));
        mCamera.Pitch(dy);
        mCamera.RotateY(dx);
    }
    mLastMousePos.x = x;
    mLastMousePos.y = y;
}

LRESULT Game::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_KEYDOWN)
        mStateStack.handleEvent(wParam);
    return D3DApp::MsgProc(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Keyboard input
// ---------------------------------------------------------------------------

/**
 * @brief Handles camera-only keyboard input (WASD) every frame.
 *
 * State-specific input (aircraft movement, menu navigation, pause) is
 * handled via StateStack::handleEvent() which is called from MsgProc
 * on WM_KEYDOWN, not here. This keeps camera movement smooth (polled
 * every frame) while state transitions are event-driven (once per press).
 *
 * @param gt  Game timer (used for delta-time scaling).
 */
void Game::OnKeyboardInput(const GameTimer& gt)
{
    const float dt = gt.DeltaTime();
    const float speed = 10.0f;

    if (GetAsyncKeyState('W') & 0x8000) mCamera.Walk(speed * dt);
    if (GetAsyncKeyState('S') & 0x8000) mCamera.Walk(-speed * dt);
    if (GetAsyncKeyState('A') & 0x8000) mCamera.Strafe(-speed * dt);
    if (GetAsyncKeyState('D') & 0x8000) mCamera.Strafe(speed * dt);
    mCamera.UpdateViewMatrix();
}

// ---------------------------------------------------------------------------
// Per-frame GPU uploads
// ---------------------------------------------------------------------------

/** @brief Placeholder for animated material logic (currently unused). */
void Game::AnimateMaterials(const GameTimer& gt)
{
    // Extend here to animate material properties over time.
}

/**
 * @brief Uploads dirty object world-matrices to the GPU constant buffer.
 * @param gt  Game timer (unused).
 */
void Game::UpdateObjectCBs(const GameTimer& gt)
{
    auto currObjectCB = mCurrFrameResource->ObjectCB.get();
    for (auto& e : mAllRitems)
    {
        if (e->NumFramesDirty > 0)
        {
            XMMATRIX world = XMLoadFloat4x4(&e->World);
            XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

            ObjectConstants objConst;
            XMStoreFloat4x4(&objConst.World, XMMatrixTranspose(world));
            XMStoreFloat4x4(&objConst.TexTransform, XMMatrixTranspose(texTransform));
            objConst.MaterialIndex = e->Mat->MatCBIndex;

            currObjectCB->CopyData(e->ObjCBIndex, objConst);
            e->NumFramesDirty--;
        }
    }
}

/**
 * @brief Uploads dirty material data to the GPU structured buffer.
 * @param gt  Game timer (unused).
 */
void Game::UpdateMaterialCBs(const GameTimer& gt)
{
    auto currMaterialBuffer = mCurrFrameResource->MaterialBuffer.get();
    for (auto& e : mMaterials)
    {
        Material* mat = e.second.get();
        if (mat->NumFramesDirty > 0)
        {
            XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

            MaterialData matData;
            matData.DiffuseAlbedo = mat->DiffuseAlbedo;
            matData.FresnelR0 = mat->FresnelR0;
            matData.Roughness = mat->Roughness;
            XMStoreFloat4x4(&matData.MatTransform, XMMatrixTranspose(matTransform));
            matData.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;
            matData.NormalMapIndex = mat->NormalSrvHeapIndex;

            currMaterialBuffer->CopyData(mat->MatCBIndex, matData);
            mat->NumFramesDirty--;
        }
    }
}

/**
 * @brief Uploads the per-pass constants (view, projection, lights) to GPU.
 * @param gt  Game timer providing total and delta time.
 */
void Game::UpdateMainPassCB(const GameTimer& gt)
{
    XMMATRIX view = mCamera.GetView();
    XMMATRIX proj = mCamera.GetProj();
    XMMATRIX viewProj = XMMatrixMultiply(view, proj);

    auto detView = XMMatrixDeterminant(view);
    auto detProj = XMMatrixDeterminant(proj);
    auto detViewProj = XMMatrixDeterminant(viewProj);

    XMMATRIX invView = XMMatrixInverse(&detView, view);
    XMMATRIX invProj = XMMatrixInverse(&detProj, proj);
    XMMATRIX invViewProj = XMMatrixInverse(&detViewProj, viewProj);

    XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
    XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
    XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));

    mMainPassCB.EyePosW = mCamera.GetPosition3f();
    mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
    mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
    mMainPassCB.NearZ = 1.0f;
    mMainPassCB.FarZ = 1000.0f;
    mMainPassCB.TotalTime = gt.TotalTime();
    mMainPassCB.DeltaTime = gt.DeltaTime();
    mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };

    // Three directional lights.
    mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f,  0.57735f };
    mMainPassCB.Lights[0].Strength = { 0.8f,      0.8f,      0.8f };
    mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f,  0.57735f };
    mMainPassCB.Lights[1].Strength = { 0.4f,      0.4f,      0.4f };
    mMainPassCB.Lights[2].Direction = { 0.0f,     -0.707f,   -0.707f };
    mMainPassCB.Lights[2].Strength = { 0.2f,      0.2f,      0.2f };

    mCurrFrameResource->PassCB->CopyData(0, mMainPassCB);
}

// ---------------------------------------------------------------------------
// One-time resource builders
// ---------------------------------------------------------------------------

/**
 * @brief Loads DDS textures (diffuse, normal, sky cube-map) from disk.
 *
 * Textures are stored in mTextures and later bound to the SRV descriptor heap.
 */
void Game::LoadTextures()
{
    std::vector<std::string>  texNames =
    {
        "bricksDiffuseMap",
        "bricksNormalMap",
        "tileDiffuseMap",
        "tileNormalMap",
        "defaultDiffuseMap",
        "defaultNormalMap",
        "skyCubeMap"
    };

    std::vector<std::wstring> texFilenames =
    {
        L"../../Textures/Eagle.dds",
        L"../../Textures/Eagle_normal.dds",
        L"../../Textures/tile.dds",
        L"../../Textures/tile_nmap.dds",
        L"../../Textures/Raptor.dds",
        L"../../Textures/Raptor_normal.dds",
        L"../../Textures/snowcube1024.dds"
    };

    for (int i = 0; i < (int)texNames.size(); ++i)
    {
        auto tex = std::make_unique<Texture>();
        tex->Name = texNames[i];
        tex->Filename = texFilenames[i];
        ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(
            md3dDevice.Get(), mCommandList.Get(),
            tex->Filename.c_str(),
            tex->Resource, tex->UploadHeap));
        mTextures[tex->Name] = std::move(tex);
    }
}

/**
 * @brief Creates the root signature with 5 slots:
 *   0 - ObjectCB         (CBV)
 *   1 - PassCB           (CBV)
 *   2 - MaterialBuffer   (SRV structured buffer)
 *   3 - Sky cube-map     (descriptor table, 1 SRV)
 *   4 - 2D textures      (descriptor table, 10 SRVs)
 */
void Game::BuildRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE texTable0;
    texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
    CD3DX12_DESCRIPTOR_RANGE texTable1;
    texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 10, 1, 0);

    CD3DX12_ROOT_PARAMETER slotRootParameter[5];
    slotRootParameter[0].InitAsConstantBufferView(0);
    slotRootParameter[1].InitAsConstantBufferView(1);
    slotRootParameter[2].InitAsShaderResourceView(0, 1);
    slotRootParameter[3].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[4].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);

    auto staticSamplers = GetStaticSamplers();
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(5, slotRootParameter,
        (UINT)staticSamplers.size(), staticSamplers.data(),
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());
    if (errorBlob != nullptr)
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    ThrowIfFailed(hr);

    ThrowIfFailed(md3dDevice->CreateRootSignature(0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

/**
 * @brief Creates the SRV descriptor heap and fills it with texture views.
 *
 * Layout:
 *   [0] bricksDiffuse  [1] bricksNormal
 *   [2] tileDiffuse    [3] tileNormal
 *   [4] defaultDiffuse [5] defaultNormal
 *   [6] skyCubeMap
 */
void Game::BuildDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 10;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc,
        IID_PPV_ARGS(&mSrvDescriptorHeap)));

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(
        mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    std::vector<ComPtr<ID3D12Resource>> tex2DList =
    {
        mTextures["bricksDiffuseMap"]->Resource,
        mTextures["bricksNormalMap"]->Resource,
        mTextures["tileDiffuseMap"]->Resource,
        mTextures["tileNormalMap"]->Resource,
        mTextures["defaultDiffuseMap"]->Resource,
        mTextures["defaultNormalMap"]->Resource
    };

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    for (UINT i = 0; i < (UINT)tex2DList.size(); ++i)
    {
        srvDesc.Format = tex2DList[i]->GetDesc().Format;
        srvDesc.Texture2D.MipLevels = tex2DList[i]->GetDesc().MipLevels;
        md3dDevice->CreateShaderResourceView(tex2DList[i].Get(), &srvDesc, hDescriptor);
        hDescriptor.Offset(1, mCbvSrvDescriptorSize);
    }

    auto skyCubeMap = mTextures["skyCubeMap"]->Resource;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.MipLevels = skyCubeMap->GetDesc().MipLevels;
    srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
    srvDesc.Format = skyCubeMap->GetDesc().Format;
    md3dDevice->CreateShaderResourceView(skyCubeMap.Get(), &srvDesc, hDescriptor);

    mSkyTexHeapIndex = (UINT)tex2DList.size();
}

/**
 * @brief Compiles shaders and defines the vertex input layout.
 *
 * Shaders used: Default.hlsl (VS + PS) and Sky.hlsl (VS + PS).
 * Input layout: Position, Normal, TexCoord, Tangent.
 */
void Game::BuildShadersAndInputLayout()
{
    mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_1");
    mShaders["skyVS"] = d3dUtil::CompileShader(L"Shaders\\Sky.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["skyPS"] = d3dUtil::CompileShader(L"Shaders\\Sky.hlsl", nullptr, "PS", "ps_5_1");

    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
}

/**
 * @brief Generates and uploads shared geometry (box, grid, sphere, cylinder)
 *        into a single vertex/index buffer named "shapeGeo".
 */
void Game::BuildShapeGeometry()
{
    GeometryGenerator geoGen;
    auto box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
    auto grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
    auto sphere = geoGen.CreateSphere(0.5f, 20, 20);
    auto cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

    UINT boxVertexOffset = 0;
    UINT gridVertexOffset = (UINT)box.Vertices.size();
    UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
    UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();

    UINT boxIndexOffset = 0;
    UINT gridIndexOffset = (UINT)box.Indices32.size();
    UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
    UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();

    SubmeshGeometry boxSubmesh;
    boxSubmesh.IndexCount = (UINT)box.Indices32.size();
    boxSubmesh.StartIndexLocation = boxIndexOffset;
    boxSubmesh.BaseVertexLocation = boxVertexOffset;

    SubmeshGeometry gridSubmesh;
    gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
    gridSubmesh.StartIndexLocation = gridIndexOffset;
    gridSubmesh.BaseVertexLocation = gridVertexOffset;

    SubmeshGeometry sphereSubmesh;
    sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
    sphereSubmesh.StartIndexLocation = sphereIndexOffset;
    sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

    SubmeshGeometry cylinderSubmesh;
    cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
    cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
    cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

    auto totalVertexCount =
        box.Vertices.size() + grid.Vertices.size() +
        sphere.Vertices.size() + cylinder.Vertices.size();

    std::vector<Vertex> vertices(totalVertexCount);
    UINT k = 0;
    for (size_t i = 0; i < box.Vertices.size(); ++i, ++k) { vertices[k].Pos = box.Vertices[i].Position;      vertices[k].Normal = box.Vertices[i].Normal;      vertices[k].TexC = box.Vertices[i].TexC;      vertices[k].TangentU = box.Vertices[i].TangentU; }
    for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k) { vertices[k].Pos = grid.Vertices[i].Position;     vertices[k].Normal = grid.Vertices[i].Normal;     vertices[k].TexC = grid.Vertices[i].TexC;     vertices[k].TangentU = grid.Vertices[i].TangentU; }
    for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k) { vertices[k].Pos = sphere.Vertices[i].Position;   vertices[k].Normal = sphere.Vertices[i].Normal;   vertices[k].TexC = sphere.Vertices[i].TexC;   vertices[k].TangentU = sphere.Vertices[i].TangentU; }
    for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k) { vertices[k].Pos = cylinder.Vertices[i].Position; vertices[k].Normal = cylinder.Vertices[i].Normal; vertices[k].TexC = cylinder.Vertices[i].TexC; vertices[k].TangentU = cylinder.Vertices[i].TangentU; }

    std::vector<std::uint16_t> indices;
    indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
    indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
    indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
    indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "shapeGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);
    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    geo->DrawArgs["box"] = boxSubmesh;
    geo->DrawArgs["grid"] = gridSubmesh;
    geo->DrawArgs["sphere"] = sphereSubmesh;
    geo->DrawArgs["cylinder"] = cylinderSubmesh;

    mGeometries[geo->Name] = std::move(geo);
}

/**
 * @brief Creates opaque and sky pipeline state objects.
 */
void Game::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;
    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    opaquePsoDesc.pRootSignature = mRootSignature.Get();
    opaquePsoDesc.VS = { reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()), mShaders["standardVS"]->GetBufferSize() };
    opaquePsoDesc.PS = { reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),   mShaders["opaquePS"]->GetBufferSize() };
    opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    opaquePsoDesc.SampleMask = UINT_MAX;
    opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    opaquePsoDesc.NumRenderTargets = 1;
    opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
    opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    opaquePsoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = opaquePsoDesc;
    skyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    skyPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    skyPsoDesc.VS = { reinterpret_cast<BYTE*>(mShaders["skyVS"]->GetBufferPointer()), mShaders["skyVS"]->GetBufferSize() };
    skyPsoDesc.PS = { reinterpret_cast<BYTE*>(mShaders["skyPS"]->GetBufferPointer()), mShaders["skyPS"]->GetBufferSize() };
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&mPSOs["sky"])));
}

/**
 * @brief Allocates one FrameResource per frame-in-flight.
 *
 * Called from GameState when the scene is built, since the object count
 * is not known until render items are registered.
 */
void Game::BuildFrameResources()
{
    for (int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(
            md3dDevice.Get(),
            1,
            (UINT)mAllRitems.size()+100,
            (UINT)mMaterials.size()));
    }
}

/**
 * @brief Flushes the GPU, clears old frame resources, and rebuilds them.
 *
 * Called by GameState after buildScene() adds render items so the
 * per-object constant buffers are correctly sized for the full scene.
 */
void Game::RebuildFrameResources()
{
    FlushCommandQueue();
    mFrameResources.clear();
    mCurrFrameResourceIndex = 0;
    for (int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(
            md3dDevice.Get(),
            1,
            (UINT)mAllRitems.size(),
            (UINT)mMaterials.size()));
    }
    mCurrFrameResource = mFrameResources[0].get();
}

/**
 * @brief Creates all materials used by the scene.
 */
void Game::BuildMaterials()
{
    auto addMaterial = [&](const std::string& name, int cbIdx,
        int diffIdx, int normIdx,
        XMFLOAT4 albedo, XMFLOAT3 fresnel, float rough)
        {
            auto mat = std::make_unique<Material>();
            mat->Name = name;
            mat->MatCBIndex = cbIdx;
            mat->DiffuseSrvHeapIndex = diffIdx;
            mat->NormalSrvHeapIndex = normIdx;
            mat->DiffuseAlbedo = albedo;
            mat->FresnelR0 = fresnel;
            mat->Roughness = rough;
            mMaterials[name] = std::move(mat);
        };

    addMaterial("bricks0", 0, 0, 1, { 1,1,1,1 }, { 0.1f,0.1f,0.1f }, 0.3f);
    addMaterial("tile0", 1, 2, 3, { 0.9f,0.9f,0.9f,1 }, { 0.2f,0.2f,0.2f }, 0.1f);
    addMaterial("mirror0", 2, 4, 5, { 0,0,0,1 }, { 0.98f,0.97f,0.95f }, 0.1f);
    addMaterial("sky", 3, 6, 7, { 1,1,1,1 }, { 0.1f,0.1f,0.1f }, 1.0f);
    addMaterial("Eagle", 4, 4, 5, { 0.3f,0.5f,0.9f,1 }, { 0.1f,0.1f,0.1f }, 0.3f);
    addMaterial("Raptor", 5, 4, 5, { 0.9f,0.2f,0.2f,1 }, { 0.1f,0.1f,0.1f }, 0.3f);
}

/**
 * @brief Stub — render items are built by GameState via World::buildScene().
 */
void Game::BuildRenderItems()
{
    // Sky sphere
    auto skyRitem = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&skyRitem->World, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
    skyRitem->TexTransform = MathHelper::Identity4x4();
    skyRitem->ObjCBIndex = 0;
    skyRitem->Mat = mMaterials["sky"].get();
    skyRitem->Geo = mGeometries["shapeGeo"].get();
    skyRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    skyRitem->IndexCount = skyRitem->Geo->DrawArgs["sphere"].IndexCount;
    skyRitem->StartIndexLocation = skyRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
    skyRitem->BaseVertexLocation = skyRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
    mSkyRitems.push_back(skyRitem.get());
    mAllRitems.push_back(std::move(skyRitem));
}

/**
 * @brief Issues indexed draw calls for a list of render items.
 * @param cmdList  The command list to record into.
 * @param ritems   The render items to draw.
 */
void Game::DrawRenderItems(ID3D12GraphicsCommandList* cmdList,
    const std::vector<RenderItem*>& ritems)
{
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    auto objectCB = mCurrFrameResource->ObjectCB->Resource();

    for (size_t i = 0; i < ritems.size(); ++i)
    {
        auto ri = ritems[i];
        auto vbv = ri->Geo->VertexBufferView();
        auto ibv = ri->Geo->IndexBufferView();

        cmdList->IASetVertexBuffers(0, 1, &vbv);
        cmdList->IASetIndexBuffer(&ibv);
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress =
            objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
        cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1,
            ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}

/**
 * @brief Returns the six standard static samplers used by the shaders.
 */
std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> Game::GetStaticSamplers()
{
    const CD3DX12_STATIC_SAMPLER_DESC pointWrap(0, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
    const CD3DX12_STATIC_SAMPLER_DESC pointClamp(1, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    const CD3DX12_STATIC_SAMPLER_DESC linearWrap(2, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
    const CD3DX12_STATIC_SAMPLER_DESC linearClamp(3, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(4, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 0.0f, 8);
    const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(5, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 0.0f, 8);
    return { pointWrap, pointClamp, linearWrap, linearClamp, anisotropicWrap, anisotropicClamp };
}