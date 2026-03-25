/**
 * @file Game.cpp
 * @brief Implementation of the Game class.
 *
 * Game is the top-level DirectX 12 application. It owns all GPU resources
 * (textures, shaders, PSOs, frame resources, constant buffers) and delegates
 * game logic to the World / scene-graph hierarchy.
 *
 * Keyboard controls:
 *   W / S / A / D  – fly the camera forward/back/left/right
 *   Arrow keys      – move the player aircraft (Eagle) in the XZ plane
 *
 * Assignment 2 changes:
 *   - processInput() replaces the old World::handlePlayerInput() call.
 *     It gets the CommandQueue from World and passes it to Player, which
 *     pushes movement commands based on current key state.
 *   - OnKeyboardInput() now only moves the camera (WASD).
 */

#include "Game.hpp"

const int gNumFrameResources = 3;

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

/**
 * @brief Constructs the Game, initialising the World with a back-pointer.
 * @param hInstance  Windows application instance handle.
 */
Game::Game(HINSTANCE hInstance)
    : D3DApp(hInstance)
    , mWorld(this)
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
 * @brief Initialises DirectX 12 resources and the game world.
 *
 * Called once at startup. Sets up the camera, loads textures, builds the
 * pipeline state, geometry, materials, render items, and frame resources.
 */
bool Game::Initialize()
{
    if (!D3DApp::Initialize())
        return false;

    // Position camera above the scene, looking down at a slight angle.
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
    BuildMaterials();
    BuildRenderItems();   // calls mWorld.buildScene() internally
    BuildFrameResources();
    BuildPSOs();

    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    FlushCommandQueue();

    return true;
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
 * @brief Per-frame update: input, world, and GPU constant-buffer uploads.
 * @param gt  Game timer providing delta time and total time.
 */
void Game::Update(const GameTimer& gt)
{
    mCurrentGt = &gt;

    OnKeyboardInput(gt);  // camera only
    processInput();        // player aircraft via command system

    mWorld.update(gt);

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
 * @param gt  Game timer (unused directly here).
 */
void Game::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;
    ThrowIfFailed(cmdListAlloc->Reset());
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // Transition back buffer: present → render target.
    auto t1 = CD3DX12_RESOURCE_BARRIER::Transition(
        CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    mCommandList->ResourceBarrier(1, &t1);

    // Clear.
    mCommandList->ClearRenderTargetView(
        CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    mCommandList->ClearDepthStencilView(
        DepthStencilView(),
        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
        1.0f, 0, 0, nullptr);

    auto bbv = CurrentBackBufferView();
    auto dsv = DepthStencilView();
    mCommandList->OMSetRenderTargets(1, &bbv, true, &dsv);

    // Bind descriptor heap and root signature.
    ID3D12DescriptorHeap* heaps[] = { mSrvDescriptorHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(heaps), heaps);
    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    // Pass constants (slot 1).
    auto passCB = mCurrFrameResource->PassCB->Resource();
    mCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

    // Structured material buffer (slot 2).
    auto matBuffer = mCurrFrameResource->MaterialBuffer->Resource();
    mCommandList->SetGraphicsRootShaderResourceView(2, matBuffer->GetGPUVirtualAddress());

    // Sky cube-map texture (slot 3).
    CD3DX12_GPU_DESCRIPTOR_HANDLE skyTex(
        mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    skyTex.Offset(mSkyTexHeapIndex, mCbvSrvDescriptorSize);
    mCommandList->SetGraphicsRootDescriptorTable(3, skyTex);

    // All 2-D textures starting at heap slot 0 (slot 4).
    mCommandList->SetGraphicsRootDescriptorTable(
        4, mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

    // Draw opaque objects.
    DrawRenderItems(mCommandList.Get(), mOpaqueRitems);

    // Switch to sky PSO and draw sky.
    mCommandList->SetPipelineState(mPSOs["sky"].Get());
    DrawRenderItems(mCommandList.Get(), mSkyRitems);

    // Transition back buffer: render target → present.
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

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

/**
 * @brief Feeds the command queue from Player each frame.
 *
 * Gets the CommandQueue from World and passes it to Player, which checks
 * key state and pushes movement commands. World drains the queue in update().
 */
void Game::processInput()
{
    CommandQueue& commands = mWorld.getCommandQueue();
    mPlayer.handleEvent(commands, *mCurrentGt);
    mPlayer.handleRealtimeInput(commands, *mCurrentGt);
}

/**
 * @brief Handles camera-only keyboard input (WASD).
 *
 * Aircraft movement is handled via the command system in processInput().
 *
 * @param gt  Game timer (used for delta-time scaling).
 */
void Game::OnKeyboardInput(const GameTimer& gt)
{
    const float dt    = gt.DeltaTime();
    const float speed = 10.0f;

    if (GetAsyncKeyState('W') & 0x8000) mCamera.Walk( speed * dt);
    if (GetAsyncKeyState('S') & 0x8000) mCamera.Walk(-speed * dt);
    if (GetAsyncKeyState('A') & 0x8000) mCamera.Strafe(-speed * dt);
    if (GetAsyncKeyState('D') & 0x8000) mCamera.Strafe( speed * dt);
    mCamera.UpdateViewMatrix();
}

// ---------------------------------------------------------------------------
// Per-frame GPU uploads
// ---------------------------------------------------------------------------

/** @brief Placeholder for animated material logic (currently unused). */
void Game::AnimateMaterials(const GameTimer& gt)
{
    // Extend here to animate material properties over time (e.g. scrolling UVs).
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
            XMMATRIX world        = XMLoadFloat4x4(&e->World);
            XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

            ObjectConstants objConst;
            XMStoreFloat4x4(&objConst.World,        XMMatrixTranspose(world));
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
            matData.DiffuseAlbedo   = mat->DiffuseAlbedo;
            matData.FresnelR0       = mat->FresnelR0;
            matData.Roughness       = mat->Roughness;
            XMStoreFloat4x4(&matData.MatTransform, XMMatrixTranspose(matTransform));
            matData.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;
            matData.NormalMapIndex  = mat->NormalSrvHeapIndex;

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
    XMMATRIX view     = mCamera.GetView();
    XMMATRIX proj     = mCamera.GetProj();
    XMMATRIX viewProj = XMMatrixMultiply(view, proj);

    auto detView     = XMMatrixDeterminant(view);
    auto detProj     = XMMatrixDeterminant(proj);
    auto detViewProj = XMMatrixDeterminant(viewProj);

    XMMATRIX invView     = XMMatrixInverse(&detView,     view);
    XMMATRIX invProj     = XMMatrixInverse(&detProj,     proj);
    XMMATRIX invViewProj = XMMatrixInverse(&detViewProj, viewProj);

    XMStoreFloat4x4(&mMainPassCB.View,        XMMatrixTranspose(view));
    XMStoreFloat4x4(&mMainPassCB.InvView,     XMMatrixTranspose(invView));
    XMStoreFloat4x4(&mMainPassCB.Proj,        XMMatrixTranspose(proj));
    XMStoreFloat4x4(&mMainPassCB.InvProj,     XMMatrixTranspose(invProj));
    XMStoreFloat4x4(&mMainPassCB.ViewProj,    XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));

    mMainPassCB.EyePosW             = mCamera.GetPosition3f();
    mMainPassCB.RenderTargetSize    = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
    mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
    mMainPassCB.NearZ               = 1.0f;
    mMainPassCB.FarZ                = 1000.0f;
    mMainPassCB.TotalTime           = gt.TotalTime();
    mMainPassCB.DeltaTime           = gt.DeltaTime();
    mMainPassCB.AmbientLight        = { 0.25f, 0.25f, 0.35f, 1.0f };

    // Three directional lights.
    mMainPassCB.Lights[0].Direction = {  0.57735f, -0.57735f,  0.57735f };
    mMainPassCB.Lights[0].Strength  = {  0.8f,      0.8f,      0.8f };
    mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f,  0.57735f };
    mMainPassCB.Lights[1].Strength  = {  0.4f,      0.4f,      0.4f };
    mMainPassCB.Lights[2].Direction = {  0.0f,     -0.707f,   -0.707f };
    mMainPassCB.Lights[2].Strength  = {  0.2f,      0.2f,      0.2f };

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
        auto tex      = std::make_unique<Texture>();
        tex->Name     = texNames[i];
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
    texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);   // sky cube

    CD3DX12_DESCRIPTOR_RANGE texTable1;
    texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 10, 1, 0);  // 2D textures

    CD3DX12_ROOT_PARAMETER slotRootParameter[5];
    slotRootParameter[0].InitAsConstantBufferView(0);
    slotRootParameter[1].InitAsConstantBufferView(1);
    slotRootParameter[2].InitAsShaderResourceView(0, 1);
    slotRootParameter[3].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[4].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);

    auto staticSamplers = GetStaticSamplers();

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
        5, slotRootParameter,
        (UINT)staticSamplers.size(), staticSamplers.data(),
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob         = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(
        &rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if (errorBlob != nullptr)
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    ThrowIfFailed(hr);

    ThrowIfFailed(md3dDevice->CreateRootSignature(
        0,
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
    srvHeapDesc.Type  = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc,
        IID_PPV_ARGS(&mSrvDescriptorHeap)));

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(
        mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    // 2D textures.
    std::vector<ComPtr<ID3D12Resource>> tex2DList =
    {
        mTextures["bricksDiffuseMap"] ->Resource,
        mTextures["bricksNormalMap"]  ->Resource,
        mTextures["tileDiffuseMap"]   ->Resource,
        mTextures["tileNormalMap"]    ->Resource,
        mTextures["defaultDiffuseMap"]->Resource,
        mTextures["defaultNormalMap"] ->Resource
    };

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping       = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip     = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    for (UINT i = 0; i < (UINT)tex2DList.size(); ++i)
    {
        srvDesc.Format              = tex2DList[i]->GetDesc().Format;
        srvDesc.Texture2D.MipLevels = tex2DList[i]->GetDesc().MipLevels;
        md3dDevice->CreateShaderResourceView(tex2DList[i].Get(), &srvDesc, hDescriptor);
        hDescriptor.Offset(1, mCbvSrvDescriptorSize);
    }

    // Sky cube-map.
    auto skyCubeMap = mTextures["skyCubeMap"]->Resource;
    srvDesc.ViewDimension                   = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MostDetailedMip     = 0;
    srvDesc.TextureCube.MipLevels           = skyCubeMap->GetDesc().MipLevels;
    srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
    srvDesc.Format = skyCubeMap->GetDesc().Format;
    md3dDevice->CreateShaderResourceView(skyCubeMap.Get(), &srvDesc, hDescriptor);

    mSkyTexHeapIndex = (UINT)tex2DList.size();  // = 6
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
    mShaders["opaquePS"]   = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_1");
    mShaders["skyVS"]      = d3dUtil::CompileShader(L"Shaders\\Sky.hlsl",     nullptr, "VS", "vs_5_1");
    mShaders["skyPS"]      = d3dUtil::CompileShader(L"Shaders\\Sky.hlsl",     nullptr, "PS", "ps_5_1");

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
 *
 * Aircraft nodes reference "shapeGeo" → "box" for their mesh.
 * The sky sphere uses "shapeGeo" → "sphere".
 */
void Game::BuildShapeGeometry()
{
    GeometryGenerator geoGen;
    auto box      = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
    auto grid     = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
    auto sphere   = geoGen.CreateSphere(0.5f, 20, 20);
    auto cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

    // Vertex offsets in the concatenated buffer.
    UINT boxVertexOffset      = 0;
    UINT gridVertexOffset     = (UINT)box.Vertices.size();
    UINT sphereVertexOffset   = gridVertexOffset   + (UINT)grid.Vertices.size();
    UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();

    // Index offsets.
    UINT boxIndexOffset      = 0;
    UINT gridIndexOffset     = (UINT)box.Indices32.size();
    UINT sphereIndexOffset   = gridIndexOffset   + (UINT)grid.Indices32.size();
    UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();

    SubmeshGeometry boxSubmesh;
    boxSubmesh.IndexCount         = (UINT)box.Indices32.size();
    boxSubmesh.StartIndexLocation = boxIndexOffset;
    boxSubmesh.BaseVertexLocation = boxVertexOffset;

    SubmeshGeometry gridSubmesh;
    gridSubmesh.IndexCount         = (UINT)grid.Indices32.size();
    gridSubmesh.StartIndexLocation = gridIndexOffset;
    gridSubmesh.BaseVertexLocation = gridVertexOffset;

    SubmeshGeometry sphereSubmesh;
    sphereSubmesh.IndexCount         = (UINT)sphere.Indices32.size();
    sphereSubmesh.StartIndexLocation = sphereIndexOffset;
    sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

    SubmeshGeometry cylinderSubmesh;
    cylinderSubmesh.IndexCount         = (UINT)cylinder.Indices32.size();
    cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
    cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

    // Pack all vertices.
    auto totalVertexCount =
        box.Vertices.size() + grid.Vertices.size() +
        sphere.Vertices.size() + cylinder.Vertices.size();

    std::vector<Vertex> vertices(totalVertexCount);
    UINT k = 0;

    for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos      = box.Vertices[i].Position;
        vertices[k].Normal   = box.Vertices[i].Normal;
        vertices[k].TexC     = box.Vertices[i].TexC;
        vertices[k].TangentU = box.Vertices[i].TangentU;
    }
    for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos      = grid.Vertices[i].Position;
        vertices[k].Normal   = grid.Vertices[i].Normal;
        vertices[k].TexC     = grid.Vertices[i].TexC;
        vertices[k].TangentU = grid.Vertices[i].TangentU;
    }
    for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos      = sphere.Vertices[i].Position;
        vertices[k].Normal   = sphere.Vertices[i].Normal;
        vertices[k].TexC     = sphere.Vertices[i].TexC;
        vertices[k].TangentU = sphere.Vertices[i].TangentU;
    }
    for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos      = cylinder.Vertices[i].Position;
        vertices[k].Normal   = cylinder.Vertices[i].Normal;
        vertices[k].TexC     = cylinder.Vertices[i].TexC;
        vertices[k].TangentU = cylinder.Vertices[i].TangentU;
    }

    // Pack all indices.
    std::vector<std::uint16_t> indices;
    indices.insert(indices.end(), std::begin(box.GetIndices16()),      std::end(box.GetIndices16()));
    indices.insert(indices.end(), std::begin(grid.GetIndices16()),     std::end(grid.GetIndices16()));
    indices.insert(indices.end(), std::begin(sphere.GetIndices16()),   std::end(sphere.GetIndices16()));
    indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size()  * sizeof(std::uint16_t);

    auto geo  = std::make_unique<MeshGeometry>();
    geo->Name = "shapeGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(
        md3dDevice.Get(), mCommandList.Get(),
        vertices.data(), vbByteSize, geo->VertexBufferUploader);
    geo->IndexBufferGPU  = d3dUtil::CreateDefaultBuffer(
        md3dDevice.Get(), mCommandList.Get(),
        indices.data(),  ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride     = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat          = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize  = ibByteSize;

    geo->DrawArgs["box"]      = boxSubmesh;
    geo->DrawArgs["grid"]     = gridSubmesh;
    geo->DrawArgs["sphere"]   = sphereSubmesh;
    geo->DrawArgs["cylinder"] = cylinderSubmesh;

    mGeometries[geo->Name] = std::move(geo);
}

/**
 * @brief Creates opaque and sky pipeline state objects.
 *
 * Sky PSO disables back-face culling and uses LESS_EQUAL depth comparison
 * so the sky renders at maximum depth without being clipped.
 */
void Game::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;
    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    opaquePsoDesc.InputLayout    = { mInputLayout.data(), (UINT)mInputLayout.size() };
    opaquePsoDesc.pRootSignature = mRootSignature.Get();
    opaquePsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
        mShaders["standardVS"]->GetBufferSize()
    };
    opaquePsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
        mShaders["opaquePS"]->GetBufferSize()
    };
    opaquePsoDesc.RasterizerState       = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    opaquePsoDesc.BlendState            = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    opaquePsoDesc.DepthStencilState     = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    opaquePsoDesc.SampleMask            = UINT_MAX;
    opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    opaquePsoDesc.NumRenderTargets      = 1;
    opaquePsoDesc.RTVFormats[0]         = mBackBufferFormat;
    opaquePsoDesc.SampleDesc.Count      = m4xMsaaState ? 4 : 1;
    opaquePsoDesc.SampleDesc.Quality    = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    opaquePsoDesc.DSVFormat             = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(
        &opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

    // Sky PSO — render inside-out, depth LESS_EQUAL.
    D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = opaquePsoDesc;
    skyPsoDesc.RasterizerState.CullMode          = D3D12_CULL_MODE_NONE;
    skyPsoDesc.DepthStencilState.DepthFunc       = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    skyPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["skyVS"]->GetBufferPointer()),
        mShaders["skyVS"]->GetBufferSize()
    };
    skyPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["skyPS"]->GetBufferPointer()),
        mShaders["skyPS"]->GetBufferSize()
    };
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(
        &skyPsoDesc, IID_PPV_ARGS(&mPSOs["sky"])));
}

/**
 * @brief Allocates one FrameResource per frame-in-flight.
 *
 * Frame count is gNumFrameResources (= 3). Each resource holds
 * a command allocator, object CB, pass CB, and material structured buffer.
 */
void Game::BuildFrameResources()
{
    for (int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(
            md3dDevice.Get(),
            1,                          // pass count
            (UINT)mAllRitems.size(),    // object count
            (UINT)mMaterials.size()));  // material count
    }
}

/**
 * @brief Creates all materials used by the scene.
 *
 * Standard NormalMap materials: bricks0, tile0, mirror0, sky.
 * Aircraft materials: Eagle (blue), Raptor (red).
 * Indices must stay contiguous and match DiffuseSrvHeapIndex layout.
 */
void Game::BuildMaterials()
{
    auto addMaterial = [&](const std::string& name, int cbIdx,
                           int diffIdx, int normIdx,
                           XMFLOAT4 albedo, XMFLOAT3 fresnel, float rough)
    {
        auto mat = std::make_unique<Material>();
        mat->Name                = name;
        mat->MatCBIndex          = cbIdx;
        mat->DiffuseSrvHeapIndex = diffIdx;
        mat->NormalSrvHeapIndex  = normIdx;
        mat->DiffuseAlbedo       = albedo;
        mat->FresnelR0           = fresnel;
        mat->Roughness           = rough;
        mMaterials[name]         = std::move(mat);
    };

    // Standard scene materials.
    addMaterial("bricks0", 0, 0, 1, {1,1,1,1},          {0.1f,0.1f,0.1f},    0.3f);
    addMaterial("tile0",   1, 2, 3, {0.9f,0.9f,0.9f,1}, {0.2f,0.2f,0.2f},    0.1f);
    addMaterial("mirror0", 2, 4, 5, {0,0,0,1},           {0.98f,0.97f,0.95f}, 0.1f);
    addMaterial("sky",     3, 6, 7, {1,1,1,1},           {0.1f,0.1f,0.1f},    1.0f);

    // Aircraft materials (reuse default diffuse/normal textures).
    addMaterial("Eagle",  4, 4, 5, {0.3f,0.5f,0.9f,1}, {0.1f,0.1f,0.1f}, 0.3f);  // blue
    addMaterial("Raptor", 5, 4, 5, {0.9f,0.2f,0.2f,1}, {0.1f,0.1f,0.1f}, 0.3f);  // red
}

/**
 * @brief Builds all render items.
 *
 * Creates the sky sphere render item, then calls mWorld.buildScene()
 * which in turn calls Aircraft::buildCurrent() to push aircraft render items.
 * Finally, populates the opaque and sky render-item lists.
 */
void Game::BuildRenderItems()
{
    // --- Sky sphere ---
    auto skyRitem = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&skyRitem->World, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
    skyRitem->TexTransform       = MathHelper::Identity4x4();
    skyRitem->ObjCBIndex         = 0;
    skyRitem->Mat                = mMaterials["sky"].get();
    skyRitem->Geo                = mGeometries["shapeGeo"].get();
    skyRitem->PrimitiveType      = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    skyRitem->IndexCount         = skyRitem->Geo->DrawArgs["sphere"].IndexCount;
    skyRitem->StartIndexLocation = skyRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
    skyRitem->BaseVertexLocation = skyRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
    mSkyRitems.push_back(skyRitem.get());
    mAllRitems.push_back(std::move(skyRitem));

    // --- Aircraft (Eagle + Raptors) via scene graph ---
    mWorld.buildScene();

    // Partition render items into sky vs opaque lists.
    for (auto& e : mAllRitems)
    {
        // Sky item is already in mSkyRitems; everything else is opaque.
        if (e->Mat != mMaterials["sky"].get())
            mOpaqueRitems.push_back(e.get());
    }
}

/**
 * @brief Issues indexed draw calls for a list of render items.
 *
 * Binds vertex/index buffers, sets per-object CB address, and draws.
 *
 * @param cmdList  The command list to record into.
 * @param ritems   The render items to draw.
 */
void Game::DrawRenderItems(ID3D12GraphicsCommandList* cmdList,
                           const std::vector<RenderItem*>& ritems)
{
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    auto objectCB      = mCurrFrameResource->ObjectCB->Resource();

    for (size_t i = 0; i < ritems.size(); ++i)
    {
        auto ri  = ritems[i];
        auto vbv = ri->Geo->VertexBufferView();
        auto ibv = ri->Geo->IndexBufferView();

        cmdList->IASetVertexBuffers(0, 1, &vbv);
        cmdList->IASetIndexBuffer(&ibv);
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress =
            objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
        cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);

        cmdList->DrawIndexedInstanced(
            ri->IndexCount, 1,
            ri->StartIndexLocation,
            ri->BaseVertexLocation, 0);
    }
}

/**
 * @brief Returns the six standard static samplers used by the shaders.
 *
 * Covers point/linear/anisotropic in both wrap and clamp modes.
 */
std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> Game::GetStaticSamplers()
{
    const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
        0, D3D12_FILTER_MIN_MAG_MIP_POINT,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  D3D12_TEXTURE_ADDRESS_MODE_WRAP,  D3D12_TEXTURE_ADDRESS_MODE_WRAP);

    const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
        1, D3D12_FILTER_MIN_MAG_MIP_POINT,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

    const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
        2, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  D3D12_TEXTURE_ADDRESS_MODE_WRAP,  D3D12_TEXTURE_ADDRESS_MODE_WRAP);

    const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
        3, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
        4, D3D12_FILTER_ANISOTROPIC,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  D3D12_TEXTURE_ADDRESS_MODE_WRAP,  D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        0.0f, 8);

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
        5, D3D12_FILTER_ANISOTROPIC,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        0.0f, 8);

    return { pointWrap, pointClamp, linearWrap, linearClamp,
             anisotropicWrap, anisotropicClamp };
}
