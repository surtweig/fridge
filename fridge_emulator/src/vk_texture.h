#pragma once

#include <vulkan/vulkan.h>
#include <cstddef>
#include <cstdint>

struct VkTexture {
    VkImage         image = VK_NULL_HANDLE;
    VkDeviceMemory  image_memory = VK_NULL_HANDLE;
    VkImageView     view = VK_NULL_HANDLE;

    VkBuffer        staging = VK_NULL_HANDLE;
    VkDeviceMemory  staging_memory = VK_NULL_HANDLE;
    VkDeviceSize    staging_size = 0;
    void*           staging_mapped = nullptr;

    VkCommandPool   cmd_pool = VK_NULL_HANDLE;
    VkCommandBuffer cmd_buf = VK_NULL_HANDLE;
    VkFence         fence = VK_NULL_HANDLE;

    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;

    uint32_t        width = 0;
    uint32_t        height = 0;
    VkFormat        format = VK_FORMAT_UNDEFINED;
};

bool VkTexture_Init(VkTexture& t, uint32_t width, uint32_t height,
                    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM);

// Upload `src_size` bytes from `src` into the staging buffer (RGBA8 layout),
// then perform a staging->device image copy on `g_Queue`. `src_size` must be
// exactly width*height*4 bytes for an RGBA8 texture.
bool VkTexture_Upload(VkTexture& t, const void* src, VkDeviceSize src_size);

void VkTexture_Shutdown(VkTexture& t);