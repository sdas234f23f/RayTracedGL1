// Copyright (c) 2022 Sultim Tsyrendashiev
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "FSR3.h"

#include <ffx_api/ffx_api.h>
#include <ffx_api/ffx_upscale.h>
#include <ffx_api/vk/ffx_api_vk.h>

#include "RenderResolutionHelper.h"
#include "RgException.h"

#include <Windows.h>
#define FSR3_TRACE(msg) OutputDebugStringA("[FSR3] " msg "\n")

namespace
{
    void FsrMessageCallback(uint32_t type, const wchar_t* msg)
    {
        char buf[512];
        snprintf(buf, sizeof(buf), "[FSR3] type=%u: %S\n", type, msg);
        OutputDebugStringA(buf);
    }

    FfxApiSurfaceFormat MapVkFormat(VkFormat fmt)
    {
        switch (fmt)
        {
        case VK_FORMAT_R16G16B16A16_SFLOAT:        return FFX_API_SURFACE_FORMAT_R16G16B16A16_FLOAT;
        case VK_FORMAT_R32G32B32A32_SFLOAT:        return FFX_API_SURFACE_FORMAT_R32G32B32A32_FLOAT;
        case VK_FORMAT_R32G32_SFLOAT:              return FFX_API_SURFACE_FORMAT_R32G32_FLOAT;
        case VK_FORMAT_R32_SFLOAT:                 return FFX_API_SURFACE_FORMAT_R32_FLOAT;
        case VK_FORMAT_R16G16_SFLOAT:              return FFX_API_SURFACE_FORMAT_R16G16_FLOAT;
        case VK_FORMAT_R16_SFLOAT:                 return FFX_API_SURFACE_FORMAT_R16_FLOAT;
        case VK_FORMAT_R8G8B8A8_UNORM:             return FFX_API_SURFACE_FORMAT_R8G8B8A8_UNORM;
        case VK_FORMAT_B8G8R8A8_UNORM:             return FFX_API_SURFACE_FORMAT_B8G8R8A8_UNORM;
        case VK_FORMAT_B10G11R11_UFLOAT_PACK32:    return FFX_API_SURFACE_FORMAT_R11G11B10_FLOAT;
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32:   return FFX_API_SURFACE_FORMAT_R10G10B10A2_UNORM;
        case VK_FORMAT_R16G16_UINT:                return FFX_API_SURFACE_FORMAT_R16G16_UINT;
        default:                                   return FFX_API_SURFACE_FORMAT_UNKNOWN;
        }
    }

    constexpr RTGL1::FramebufferImageIndex OUTPUT_IMAGE_INDEX = RTGL1::FB_IMAGE_INDEX_UPSCALED_PONG;

    FfxApiResource ToFfxApiResource(
        RTGL1::FramebufferImageIndex fbImage, uint32_t frameIndex,
        const RTGL1::Framebuffers& framebuffers,
        const RTGL1::ResolutionState& resolutionState)
    {
        auto [image, view, format, sz] = framebuffers.GetImageHandles(fbImage, frameIndex, resolutionState);

        FfxApiResource res = {};
        res.resource = (void*)image;
        res.description.type     = FFX_API_RESOURCE_TYPE_TEXTURE2D;
        res.description.format   = MapVkFormat(format);
        res.description.width    = sz.width;
        res.description.height   = sz.height;
        res.description.depth    = 1;
        res.description.mipCount = 1;
        res.description.flags    = FFX_API_RESOURCE_FLAGS_NONE;
        res.description.usage    = (fbImage == OUTPUT_IMAGE_INDEX)
            ? FFX_API_RESOURCE_USAGE_UAV
            : FFX_API_RESOURCE_USAGE_READ_ONLY;
        res.state = (fbImage == OUTPUT_IMAGE_INDEX)
            ? FFX_API_RESOURCE_STATE_UNORDERED_ACCESS
            : FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ;
        return res;
    }

    template <size_t N>
    void InsertBarriers(
        VkCommandBuffer cmd, uint32_t frameIndex,
        RTGL1::Framebuffers& framebuffers,
        const RTGL1::FramebufferImageIndex (&inputsAndOutput)[N],
        bool isBackwards)
    {
        assert(std::find(std::begin(inputsAndOutput), std::end(inputsAndOutput), OUTPUT_IMAGE_INDEX) != std::end(inputsAndOutput));

        VkImageMemoryBarrier2 barriers[N];
        for (size_t i = 0; i < N; i++)
        {
            auto& b = barriers[i];
            b = {};
            b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            b.srcStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            b.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT;
            b.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            b.dstAccessMask = inputsAndOutput[i] == OUTPUT_IMAGE_INDEX ? VK_ACCESS_2_SHADER_WRITE_BIT : VK_ACCESS_2_SHADER_READ_BIT;
            b.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            b.newLayout = inputsAndOutput[i] == OUTPUT_IMAGE_INDEX ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.image = framebuffers.GetImage(inputsAndOutput[i], frameIndex);
            b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            b.subresourceRange.baseMipLevel = 0;
            b.subresourceRange.levelCount = 1;
            b.subresourceRange.baseArrayLayer = 0;
            b.subresourceRange.layerCount = 1;

            if (isBackwards)
            {
                std::swap(b.srcStageMask, b.dstStageMask);
                std::swap(b.srcAccessMask, b.dstAccessMask);
                std::swap(b.oldLayout, b.newLayout);
                std::swap(b.srcQueueFamilyIndex, b.dstQueueFamilyIndex);
            }
        }

        VkDependencyInfoKHR depInfo = {};
        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
        depInfo.imageMemoryBarrierCount = static_cast<uint32_t>(N);
        depInfo.pImageMemoryBarriers = barriers;

        RTGL1::svkCmdPipelineBarrier2KHR(cmd, &depInfo);
    }
}

RTGL1::FSR3::FSR3(VkDevice _device, VkPhysicalDevice _physDevice)
    : m_device(_device)
    , m_physDevice(_physDevice)
    , m_context(nullptr)
    , m_renderWidth(0)
    , m_renderHeight(0)
    , m_displayWidth(0)
    , m_displayHeight(0)
{
    FSR3_TRACE("Constructor");
}

RTGL1::FSR3::~FSR3()
{
    if (m_context)
    {
        ffxDestroyContext(&m_context, nullptr);
        m_context = nullptr;
    }
}

void RTGL1::FSR3::OnFramebuffersSizeChange(const ResolutionState& resolutionState)
{
    if (m_context)
    {
        ffxDestroyContext(&m_context, nullptr);
        m_context = nullptr;
    }

    m_renderWidth  = resolutionState.renderWidth;
    m_renderHeight = resolutionState.renderHeight;
    m_displayWidth  = resolutionState.upscaledWidth;
    m_displayHeight = resolutionState.upscaledHeight;

    char buf[128];
    snprintf(buf, sizeof(buf), "[FSR3] OnFramebuffersSizeChange: render=%ux%u upscale=%ux%u\n",
        m_renderWidth, m_renderHeight, m_displayWidth, m_displayHeight);
    OutputDebugStringA(buf);

    ffxCreateBackendVKDesc backendDesc = {};
    backendDesc.header.type      = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_VK;
    backendDesc.vkDevice         = m_device;
    backendDesc.vkPhysicalDevice = m_physDevice;
    backendDesc.vkDeviceProcAddr = vkGetDeviceProcAddr;

    ffxCreateContextDescUpscale upscaleDesc = {};
    upscaleDesc.header.type   = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE;
    upscaleDesc.header.pNext  = &backendDesc.header;
    upscaleDesc.flags         = FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE;
    upscaleDesc.maxRenderSize  = { m_renderWidth, m_renderHeight };
    upscaleDesc.maxUpscaleSize = { m_displayWidth, m_displayHeight };
    upscaleDesc.fpMessage      = FsrMessageCallback;

    ffxReturnCode_t r = ffxCreateContext(&m_context, &upscaleDesc.header, nullptr);
    if (r != FFX_API_RETURN_OK)
    {
        m_context = nullptr;
        throw RgException(RG_GRAPHICS_API_ERROR, "Failed to create FSR 3.1 context");
    }

    // Update static context for GetJitter (which is a static method)
    s_contextForJitter = m_context;
}

RTGL1::FramebufferImageIndex RTGL1::FSR3::Apply(
    VkCommandBuffer cmd, uint32_t frameIndex,
    const std::shared_ptr<Framebuffers>& framebuffers,
    const RenderResolutionHelper& renderResolution,
    RgFloat2D jitterOffset,
    float timeDelta,
    float nearPlane, float farPlane, float fovVerticalRad)
{
    if (!m_context)
    {
        OutputDebugStringA("[FSR3] Apply SKIPPED — no context\n");
        return FB_IMAGE_INDEX_FINAL;
    }

    using FI = RTGL1::FramebufferImageIndex;

    FI rs[] =
    {
        FI::FB_IMAGE_INDEX_FINAL,
        FI::FB_IMAGE_INDEX_DEPTH_NDC,
        FI::FB_IMAGE_INDEX_MOTION_DLSS,
        OUTPUT_IMAGE_INDEX
    };
    InsertBarriers(cmd, frameIndex, *framebuffers, rs, false);

    ffxDispatchDescUpscale info = {};
    info.header.type       = FFX_API_DISPATCH_DESC_TYPE_UPSCALE;
    info.commandList       = cmd;
    info.color             = ToFfxApiResource(FI::FB_IMAGE_INDEX_FINAL,           frameIndex, *framebuffers, renderResolution.GetResolutionState());
    info.depth             = ToFfxApiResource(FI::FB_IMAGE_INDEX_DEPTH_NDC,       frameIndex, *framebuffers, renderResolution.GetResolutionState());
    info.motionVectors     = ToFfxApiResource(FI::FB_IMAGE_INDEX_MOTION_DLSS,     frameIndex, *framebuffers, renderResolution.GetResolutionState());
    info.exposure          = {};
    info.reactive          = {};
    info.transparencyAndComposition = {};
    info.output            = ToFfxApiResource(OUTPUT_IMAGE_INDEX, frameIndex, *framebuffers, renderResolution.GetResolutionState());
    info.jitterOffset.x    = jitterOffset.data[0];
    info.jitterOffset.y    = jitterOffset.data[1];
    info.motionVectorScale.x = static_cast<float>(renderResolution.GetResolutionState().renderWidth);
    info.motionVectorScale.y = static_cast<float>(renderResolution.GetResolutionState().renderHeight);
    info.renderSize.width  = renderResolution.GetResolutionState().renderWidth;
    info.renderSize.height = renderResolution.GetResolutionState().renderHeight;
    info.upscaleSize.width  = m_displayWidth;
    info.upscaleSize.height = m_displayHeight;
    info.enableSharpening  = false;
    info.sharpness         = 0.0f;
    info.frameTimeDelta    = timeDelta;
    info.preExposure       = 1.0f;
    info.reset             = false;
    info.cameraNear        = nearPlane;
    info.cameraFar         = farPlane;
    info.cameraFovAngleVertical = fovVerticalRad;
    info.viewSpaceToMetersFactor = 1.0f;
    info.flags             = 0;

    ffxReturnCode_t r = ffxDispatch(&m_context, &info.header);
    if (r != FFX_API_RETURN_OK)
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "[FSR3] ffxDispatch FAILED: r=%u\n", (unsigned)r);
        OutputDebugStringA(buf);
        return FB_IMAGE_INDEX_FINAL;
    }

    static int frameCount = 0;
    if (++frameCount % 60 == 1)
    {
        auto colorRes = ToFfxApiResource(FI::FB_IMAGE_INDEX_FINAL,       frameIndex, *framebuffers, renderResolution.GetResolutionState());
        auto depthRes = ToFfxApiResource(FI::FB_IMAGE_INDEX_DEPTH_NDC,   frameIndex, *framebuffers, renderResolution.GetResolutionState());
        auto mvRes    = ToFfxApiResource(FI::FB_IMAGE_INDEX_MOTION_DLSS, frameIndex, *framebuffers, renderResolution.GetResolutionState());
        auto outRes   = ToFfxApiResource(OUTPUT_IMAGE_INDEX,              frameIndex, *framebuffers, renderResolution.GetResolutionState());

        char buf[384];
        snprintf(buf, sizeof(buf), "[FSR3] frame %d | render=%ux%u upscale=%ux%u | "
            "color=%ux%u fmt=%u mv=%ux%u fmt=%u depth=%ux%u fmt=%u out=%ux%u fmt=%u | jitter=(%.4f,%.4f) dt=%.2f\n",
            frameCount,
            renderResolution.GetResolutionState().renderWidth, renderResolution.GetResolutionState().renderHeight,
            m_displayWidth, m_displayHeight,
            colorRes.description.width, colorRes.description.height, (unsigned)colorRes.description.format,
            mvRes.description.width, mvRes.description.height, (unsigned)mvRes.description.format,
            depthRes.description.width, depthRes.description.height, (unsigned)depthRes.description.format,
            outRes.description.width, outRes.description.height, (unsigned)outRes.description.format,
            jitterOffset.data[0], jitterOffset.data[1],
            timeDelta);
        OutputDebugStringA(buf);
    }

    InsertBarriers(cmd, frameIndex, *framebuffers, rs, true);

    return OUTPUT_IMAGE_INDEX;
}

RgFloat2D RTGL1::FSR3::GetJitter(const ResolutionState& resolutionState, uint32_t frameId)
{
    if (!s_contextForJitter)
    {
        static bool once = false;
        if (!once) { OutputDebugStringA("[FSR3] GetJitter SKIPPED — no context\n"); once = true; }
        return { 0, 0 };
    }

    ffxQueryDescUpscaleGetJitterPhaseCount phaseCountDesc = {};
    phaseCountDesc.header.type     = FFX_API_QUERY_DESC_TYPE_UPSCALE_GETJITTERPHASECOUNT;
    phaseCountDesc.renderWidth     = resolutionState.renderWidth;
    phaseCountDesc.displayWidth    = resolutionState.upscaledWidth;
    int32_t phaseCount = 0;
    phaseCountDesc.pOutPhaseCount  = &phaseCount;
    ffxQuery(&s_contextForJitter, &phaseCountDesc.header);

    if (phaseCount <= 0)
    {
        return { 0, 0 };
    }

    RgFloat2D jitter = {};
    ffxQueryDescUpscaleGetJitterOffset offsetDesc = {};
    offsetDesc.header.type  = FFX_API_QUERY_DESC_TYPE_UPSCALE_GETJITTEROFFSET;
    offsetDesc.index        = static_cast<int32_t>(frameId % phaseCount);
    offsetDesc.phaseCount   = phaseCount;
    offsetDesc.pOutX        = &jitter.data[0];
    offsetDesc.pOutY        = &jitter.data[1];
    ffxQuery(&s_contextForJitter, &offsetDesc.header);

    return jitter;
}