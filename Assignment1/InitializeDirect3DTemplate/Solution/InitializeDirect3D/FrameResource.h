#pragma once

#include "../../Common/d3dUtil.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"

/**
 * @brief Per-object GPU constant buffer data.
 *
 * Uploaded once per object per dirty frame. Contains the object's
 * world matrix, UV transform, and an index into the material buffer.
 */
struct ObjectConstants
{
    DirectX::XMFLOAT4X4 World        = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
    UINT MaterialIndex;
    UINT ObjPad0;
    UINT ObjPad1;
    UINT ObjPad2;
};

/**
 * @brief Per-pass GPU constant buffer data.
 *
 * Contains camera matrices, eye position, render-target size,
 * timing data, ambient light, and the directional light array.
 * Uploaded once per frame.
 */
struct PassConstants
{
    DirectX::XMFLOAT4X4 View        = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvView     = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 Proj        = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvProj     = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 ViewProj    = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT3   EyePosW     = { 0.0f, 0.0f, 0.0f };
    float               cbPerObjectPad1 = 0.0f;
    DirectX::XMFLOAT2   RenderTargetSize    = { 0.0f, 0.0f };
    DirectX::XMFLOAT2   InvRenderTargetSize = { 0.0f, 0.0f };
    float NearZ      = 0.0f;
    float FarZ       = 0.0f;
    float TotalTime  = 0.0f;
    float DeltaTime  = 0.0f;

    DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };

    /// Directional, point, and spot lights (up to MaxLights total).
    Light Lights[MaxLights];
};

/**
 * @brief Per-material GPU structured buffer entry.
 *
 * Stored in a StructuredBuffer on the GPU and indexed via MaterialIndex
 * in ObjectConstants. Avoids a constant-buffer per material.
 */
struct MaterialData
{
    DirectX::XMFLOAT4   DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3   FresnelR0     = { 0.01f, 0.01f, 0.01f };
    float               Roughness     = 0.5f;
    DirectX::XMFLOAT4X4 MatTransform  = MathHelper::Identity4x4();
    UINT DiffuseMapIndex = 0;
    UINT NormalMapIndex  = 0;
    UINT MaterialPad1;
    UINT MaterialPad2;
};

/**
 * @brief Vertex layout matching the HLSL input signature.
 *
 * Position, Normal, TexCoord, TangentU must match the input-layout
 * defined in Game::BuildShadersAndInputLayout().
 */
struct Vertex
{
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT2 TexC;
    DirectX::XMFLOAT3 TangentU;
};

/**
 * @brief Stores per-frame GPU resources to avoid CPU/GPU synchronisation stalls.
 *
 * The application keeps gNumFrameResources (= 3) of these so the CPU can
 * record commands for frame N+1 while the GPU is still processing frame N.
 * Each FrameResource owns:
 *   - A command allocator
 *   - A per-pass constant buffer
 *   - A per-object constant buffer
 *   - A per-material structured buffer
 */
struct FrameResource
{
public:
    /**
     * @brief Allocates GPU upload heaps for the pass, object, and material buffers.
     * @param device        The D3D12 device.
     * @param passCount     Number of render passes (usually 1).
     * @param objectCount   Number of render items.
     * @param materialCount Number of materials.
     */
    FrameResource(ID3D12Device* device, UINT passCount,
                  UINT objectCount, UINT materialCount);

    FrameResource(const FrameResource& rhs) = delete;
    FrameResource& operator=(const FrameResource& rhs) = delete;
    ~FrameResource();

    /// Command allocator — reset only after GPU finishes all commands referencing it.
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

    /// Per-pass constants (view, proj, lights).
    std::unique_ptr<UploadBuffer<PassConstants>>   PassCB       = nullptr;

    /// Per-object constants (world matrix, material index).
    std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB     = nullptr;

    /// Per-material structured buffer (albedo, roughness, texture indices).
    std::unique_ptr<UploadBuffer<MaterialData>>    MaterialBuffer = nullptr;

    /// Fence value marking when this frame's commands have completed on the GPU.
    UINT64 Fence = 0;
};
