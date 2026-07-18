#pragma once

#include <vulkan/vulkan.h>

extern VkAllocationCallbacks* g_Allocator;
extern VkInstance               g_Instance;
extern VkPhysicalDevice         g_PhysicalDevice;
extern VkDevice                 g_Device;
extern uint32_t                 g_QueueFamily;
extern VkQueue                  g_Queue;
extern VkPipelineCache          g_PipelineCache;
extern VkDescriptorPool         g_DescriptorPool;