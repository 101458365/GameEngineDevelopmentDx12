/**
 * @file FrameResource.cpp
 * @brief Implementation of the FrameResource struct.
 */

#include "FrameResource.h"

/**
 * @brief Allocates the command allocator and upload buffers for one frame slot.
 *
 * @param device        The D3D12 device used to create GPU resources.
 * @param passCount     Number of per-pass constant buffers (normally 1).
 * @param objectCount   Number of per-object constant buffers (one per render item).
 * @param materialCount Number of material entries in the structured buffer.
 */
FrameResource::FrameResource(ID3D12Device* device,
                             UINT passCount,
                             UINT objectCount,
                             UINT materialCount)
{
    ThrowIfFailed(device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));

    PassCB         = std::make_unique<UploadBuffer<PassConstants>>  (device, passCount,     true);
    ObjectCB       = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount,   true);
    MaterialBuffer = std::make_unique<UploadBuffer<MaterialData>>   (device, materialCount, false);
}

FrameResource::~FrameResource()
{
}
