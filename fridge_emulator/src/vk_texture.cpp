#include "vk_texture.h"
#include "vulkan_globals.h"
#include "imgui_impl_vulkan.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static uint32_t FindMemoryType(uint32_t type_bits, VkMemoryPropertyFlags props)
{
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(g_PhysicalDevice, &mem_props);
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i)
    {
        if ((type_bits & (1u << i))
            && (mem_props.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    return UINT32_MAX;
}

static bool BeginOneTimeCommands(VkTexture& t)
{
    VkResult err = vkResetFences(g_Device, 1, &t.fence);
    if (err != VK_SUCCESS) { std::fprintf(stderr, "[vk_texture] resetFences: %d\n", err); return false; }

    err = vkResetCommandPool(g_Device, t.cmd_pool, 0);
    if (err != VK_SUCCESS) { std::fprintf(stderr, "[vk_texture] resetCommandPool: %d\n", err); return false; }

    VkCommandBufferBeginInfo bi = {};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(t.cmd_buf, &bi);
    if (err != VK_SUCCESS) { std::fprintf(stderr, "[vk_texture] begin: %d\n", err); return false; }
    return true;
}

static bool EndSubmitWait(VkTexture& t)
{
    VkResult err = vkEndCommandBuffer(t.cmd_buf);
    if (err != VK_SUCCESS) { std::fprintf(stderr, "[vk_texture] end: %d\n", err); return false; }

    VkSubmitInfo si = {};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &t.cmd_buf;
    err = vkQueueSubmit(g_Queue, 1, &si, t.fence);
    if (err != VK_SUCCESS) { std::fprintf(stderr, "[vk_texture] submit: %d\n", err); return false; }

    err = vkWaitForFences(g_Device, 1, &t.fence, VK_TRUE, UINT64_MAX);
    if (err != VK_SUCCESS) { std::fprintf(stderr, "[vk_texture] wait: %d\n", err); return false; }
    return true;
}

static void TransitionLayout(VkTexture& t, VkImageLayout from, VkImageLayout to)
{
    VkImageMemoryBarrier b = {};
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.oldLayout = from;
    b.newLayout = to;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = t.image;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.baseMipLevel = 0;
    b.subresourceRange.levelCount = 1;
    b.subresourceRange.baseArrayLayer = 0;
    b.subresourceRange.layerCount = 1;

    VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    if (from == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        && to == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (from == VK_IMAGE_LAYOUT_UNDEFINED
             && to == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        b.srcAccessMask = 0;
        b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (from == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
             && to == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        b.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }

    vkCmdPipelineBarrier(t.cmd_buf, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &b);
}

bool VkTexture_Init(VkTexture& t, uint32_t width, uint32_t height, VkFormat format)
{
    t.width = width;
    t.height = height;
    t.format = format;

    VkImageCreateInfo ici = {};
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = format;
    ici.extent.width = width;
    ici.extent.height = height;
    ici.extent.depth = 1;
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkResult err = vkCreateImage(g_Device, &ici, g_Allocator, &t.image);
    if (err != VK_SUCCESS) { std::fprintf(stderr, "[vk_texture] createImage: %d\n", err); return false; }

    {
        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(g_Device, t.image, &req);
        uint32_t type = FindMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (type == UINT32_MAX) { std::fprintf(stderr, "[vk_texture] no device-local memory\n"); return false; }
        VkMemoryAllocateInfo ai = {};
        ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = type;
        err = vkAllocateMemory(g_Device, &ai, g_Allocator, &t.image_memory);
        if (err != VK_SUCCESS) { std::fprintf(stderr, "[vk_texture] allocImageMem: %d\n", err); return false; }
        err = vkBindImageMemory(g_Device, t.image, t.image_memory, 0);
        if (err != VK_SUCCESS) { std::fprintf(stderr, "[vk_texture] bindImage: %d\n", err); return false; }
    }

    {
        VkImageViewCreateInfo vci = {};
        vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vci.image = t.image;
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format = format;
        vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vci.subresourceRange.baseMipLevel = 0;
        vci.subresourceRange.levelCount = 1;
        vci.subresourceRange.baseArrayLayer = 0;
        vci.subresourceRange.layerCount = 1;
        err = vkCreateImageView(g_Device, &vci, g_Allocator, &t.view);
        if (err != VK_SUCCESS) { std::fprintf(stderr, "[vk_texture] createView: %d\n", err); return false; }
    }

    {
        t.staging_size = VkDeviceSize(width) * height * 4;
        VkBufferCreateInfo bci = {};
        bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bci.size = t.staging_size;
        bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        err = vkCreateBuffer(g_Device, &bci, g_Allocator, &t.staging);
        if (err != VK_SUCCESS) { std::fprintf(stderr, "[vk_texture] createBuffer: %d\n", err); return false; }

        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(g_Device, t.staging, &req);
        uint32_t type = FindMemoryType(req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (type == UINT32_MAX) { std::fprintf(stderr, "[vk_texture] no host-visible memory\n"); return false; }
        VkMemoryAllocateInfo ai = {};
        ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = type;
        err = vkAllocateMemory(g_Device, &ai, g_Allocator, &t.staging_memory);
        if (err != VK_SUCCESS) { std::fprintf(stderr, "[vk_texture] allocBufferMem: %d\n", err); return false; }
        err = vkBindBufferMemory(g_Device, t.staging, t.staging_memory, 0);
        if (err != VK_SUCCESS) { std::fprintf(stderr, "[vk_texture] bindBuffer: %d\n", err); return false; }

        err = vkMapMemory(g_Device, t.staging_memory, 0, t.staging_size, 0, &t.staging_mapped);
        if (err != VK_SUCCESS) { std::fprintf(stderr, "[vk_texture] map: %d\n", err); return false; }
    }

    {
        VkCommandPoolCreateInfo ci = {};
        ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        ci.queueFamilyIndex = g_QueueFamily;
        err = vkCreateCommandPool(g_Device, &ci, g_Allocator, &t.cmd_pool);
        if (err != VK_SUCCESS) { std::fprintf(stderr, "[vk_texture] createCmdPool: %d\n", err); return false; }

        VkCommandBufferAllocateInfo ai = {};
        ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool = t.cmd_pool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        err = vkAllocateCommandBuffers(g_Device, &ai, &t.cmd_buf);
        if (err != VK_SUCCESS) { std::fprintf(stderr, "[vk_texture] allocCmdBuf: %d\n", err); return false; }

        VkFenceCreateInfo fi = {};
        fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        err = vkCreateFence(g_Device, &fi, g_Allocator, &t.fence);
        if (err != VK_SUCCESS) { std::fprintf(stderr, "[vk_texture] createFence: %d\n", err); return false; }
    }

    if (!BeginOneTimeCommands(t)) return false;
    TransitionLayout(t, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (!EndSubmitWait(t)) return false;

    t.descriptor_set = ImGui_ImplVulkan_AddTexture(t.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (t.descriptor_set == VK_NULL_HANDLE) { std::fprintf(stderr, "[vk_texture] AddTexture failed\n"); return false; }

    return true;
}

bool VkTexture_Upload(VkTexture& t, const void* src, VkDeviceSize src_size)
{
    if (!t.staging_mapped || src_size > t.staging_size)
        return false;
    std::memcpy(t.staging_mapped, src, src_size);

    if (!BeginOneTimeCommands(t)) return false;

    TransitionLayout(t, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { t.width, t.height, 1 };
    vkCmdCopyBufferToImage(t.cmd_buf, t.staging, t.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    TransitionLayout(t, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    return EndSubmitWait(t);
}

void VkTexture_Shutdown(VkTexture& t)
{
    if (t.descriptor_set != VK_NULL_HANDLE)
    {
        ImGui_ImplVulkan_RemoveTexture(t.descriptor_set);
        t.descriptor_set = VK_NULL_HANDLE;
    }
    if (t.fence != VK_NULL_HANDLE) { vkDestroyFence(g_Device, t.fence, g_Allocator); t.fence = VK_NULL_HANDLE; }
    if (t.cmd_pool != VK_NULL_HANDLE)
    {
        vkFreeCommandBuffers(g_Device, t.cmd_pool, 1, &t.cmd_buf);
        vkDestroyCommandPool(g_Device, t.cmd_pool, g_Allocator);
        t.cmd_pool = VK_NULL_HANDLE;
        t.cmd_buf = VK_NULL_HANDLE;
    }
    if (t.staging_mapped) { vkUnmapMemory(g_Device, t.staging_memory); t.staging_mapped = nullptr; }
    if (t.staging != VK_NULL_HANDLE) { vkDestroyBuffer(g_Device, t.staging, g_Allocator); t.staging = VK_NULL_HANDLE; }
    if (t.staging_memory != VK_NULL_HANDLE) { vkFreeMemory(g_Device, t.staging_memory, g_Allocator); t.staging_memory = VK_NULL_HANDLE; }
    if (t.view != VK_NULL_HANDLE) { vkDestroyImageView(g_Device, t.view, g_Allocator); t.view = VK_NULL_HANDLE; }
    if (t.image != VK_NULL_HANDLE) { vkDestroyImage(g_Device, t.image, g_Allocator); t.image = VK_NULL_HANDLE; }
    if (t.image_memory != VK_NULL_HANDLE) { vkFreeMemory(g_Device, t.image_memory, g_Allocator); t.image_memory = VK_NULL_HANDLE; }
}