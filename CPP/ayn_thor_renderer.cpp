#include "ayn_thor_renderer.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/rd_texture_format.hpp>
#include <godot_cpp/classes/time.hpp>
#include <mutex>

#ifdef __ANDROID__
#include <android/native_window.h>
extern ANativeWindow* g_second_window;
extern std::mutex g_window_mutex;
#else
extern void* g_second_window;
extern std::mutex g_window_mutex;
#endif

namespace godot {

void AynThorRenderer::_bind_methods() {
    ClassDB::bind_method(D_METHOD("is_window_available"), &AynThorRenderer::is_window_available);
    ClassDB::bind_method(D_METHOD("draw_viewport_texture", "texture_rid"), &AynThorRenderer::draw_viewport_texture);
    ClassDB::bind_method(D_METHOD("fill_color", "r", "g", "b"), &AynThorRenderer::fill_color);
    ClassDB::bind_method(D_METHOD("get_second_screen_size"), &AynThorRenderer::get_second_screen_size);

    ClassDB::bind_method(D_METHOD("set_target_fps", "fps"), &AynThorRenderer::set_target_fps);
    ClassDB::bind_method(D_METHOD("get_target_fps"), &AynThorRenderer::get_target_fps);
    
    ClassDB::bind_method(D_METHOD("set_rotation_degrees", "degrees"), &AynThorRenderer::set_rotation_degrees);
    ClassDB::bind_method(D_METHOD("get_rotation_degrees"), &AynThorRenderer::get_rotation_degrees);

    ADD_PROPERTY(PropertyInfo(Variant::INT, "target_fps"), "set_target_fps", "get_target_fps");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "rotation_degrees"), "set_rotation_degrees", "get_rotation_degrees");
}

AynThorRenderer::AynThorRenderer() {}

AynThorRenderer::~AynThorRenderer() {
    _cleanup_vulkan();
}

void AynThorRenderer::set_target_fps(int p_fps) { target_fps = p_fps; }
int AynThorRenderer::get_target_fps() const { return target_fps; }

void AynThorRenderer::set_rotation_degrees(int p_degrees) { rotation_degrees = p_degrees; }
int AynThorRenderer::get_rotation_degrees() const { return rotation_degrees; }

bool AynThorRenderer::is_window_available() {
    std::lock_guard<std::mutex> lock(g_window_mutex);
    return g_second_window != nullptr;
}

Vector2i AynThorRenderer::get_second_screen_size() {
    std::lock_guard<std::mutex> lock(g_window_mutex);
#ifdef __ANDROID__
    if (g_second_window) {
        return Vector2i(ANativeWindow_getWidth(g_second_window), ANativeWindow_getHeight(g_second_window));
    }
#endif
    return Vector2i(0, 0);
}

void AynThorRenderer::_init_vulkan() {
#ifdef __ANDROID__
    if (initialized) return;

    g_window_mutex.lock();
    if (!g_second_window) {
        g_window_mutex.unlock();
        return;
    }
    last_window = g_second_window;
    g_window_mutex.unlock();

    RenderingServer* rs = RenderingServer::get_singleton();
    if (!rs) return;
    RenderingDevice* rd = rs->get_rendering_device();
    if (!rd) return;

    vk_instance = (VkInstance)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_INSTANCE, RID(), 0);
    vk_device = (VkDevice)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_DEVICE, RID(), 0);
    vk_physical_device = (VkPhysicalDevice)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_PHYSICAL_DEVICE, RID(), 0);
    vk_queue = (VkQueue)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_QUEUE, RID(), 0);
    vk_queue_family_index = (uint32_t)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_QUEUE_FAMILY_INDEX, RID(), 0);

    if (!vk_device || !vk_instance) {
        UtilityFunctions::printerr("AynThorPlugin: Vulkan device or instance is null. Is OpenGL Compatibility mode active? Plugin requires Vulkan.");
        return;
    }

    VkAndroidSurfaceCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    createInfo.window = (ANativeWindow*)last_window;

    VkResult res = vkCreateAndroidSurfaceKHR(vk_instance, &createInfo, nullptr, &surface);
    if (res != VK_SUCCESS) {
        return;
    }

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = vk_queue_family_index;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(vk_device, &poolInfo, nullptr, &command_pool) != VK_SUCCESS) {
        return;
    }

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = command_pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    vkAllocateCommandBuffers(vk_device, &allocInfo, &command_buffer);

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    vkCreateSemaphore(vk_device, &semaphoreInfo, nullptr, &image_available_semaphore);
    vkCreateSemaphore(vk_device, &semaphoreInfo, nullptr, &render_finished_semaphore);
    vkCreateFence(vk_device, &fenceInfo, nullptr, &in_flight_fence);

    _create_swapchain();

    initialized = true;
#endif
}

void AynThorRenderer::_create_swapchain() {
#ifdef __ANDROID__
    if (!surface) return;

    int32_t window_w = ANativeWindow_getWidth((ANativeWindow*)last_window);
    int32_t window_h = ANativeWindow_getHeight((ANativeWindow*)last_window);

    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_physical_device, surface, &capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vk_physical_device, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    if (formatCount > 0) {
        vkGetPhysicalDeviceSurfaceFormatsKHR(vk_physical_device, surface, &formatCount, formats.data());
    }

    swapchain_image_format = VK_FORMAT_R8G8B8A8_UNORM;
    VkColorSpaceKHR target_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    bool format_found = false;
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_R8G8B8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            format_found = true;
            break;
        }
    }
    if (!format_found && formatCount > 0) {
        swapchain_image_format = formats[0].format;
        target_color_space = formats[0].colorSpace;
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(vk_physical_device, surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    if (presentModeCount > 0) {
        vkGetPhysicalDeviceSurfacePresentModesKHR(vk_physical_device, surface, &presentModeCount, presentModes.data());
    }

    VkPresentModeKHR target_present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto& mode : presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            target_present_mode = mode;
            break;
        }
    }

    VkSurfaceTransformFlagBitsKHR target_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    if (rotation_degrees == 90) target_transform = VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR;
    else if (rotation_degrees == 180) target_transform = VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR;
    else if (rotation_degrees == 270) target_transform = VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR;

    bool transform_supported = (capabilities.supportedTransforms & target_transform);

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && createInfo.minImageCount > capabilities.maxImageCount) {
        createInfo.minImageCount = capabilities.maxImageCount;
    }
    createInfo.imageFormat = swapchain_image_format;
    createInfo.imageColorSpace = target_color_space;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = target_present_mode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (transform_supported) {
        createInfo.preTransform = target_transform;
        if (rotation_degrees == 90 || rotation_degrees == 270) {
            width = window_h;
            height = window_w;
        } else {
            width = window_w;
            height = window_h;
        }
    } else {
        createInfo.preTransform = capabilities.currentTransform;
        width = window_w;
        height = window_h;
    }

    createInfo.imageExtent = {(uint32_t)width, (uint32_t)height};

    VkResult res = vkCreateSwapchainKHR(vk_device, &createInfo, nullptr, &swapchain);
    if (res != VK_SUCCESS) {
        return;
    }

    uint32_t imageCount;
    vkGetSwapchainImagesKHR(vk_device, swapchain, &imageCount, nullptr);
    swapchain_images.resize(imageCount);
    vkGetSwapchainImagesKHR(vk_device, swapchain, &imageCount, swapchain_images.data());
#endif
}

void AynThorRenderer::draw_viewport_texture(RID texture_rid) {
#ifdef __ANDROID__
    if (target_fps > 0) {
        uint64_t current_time = Time::get_singleton()->get_ticks_usec();
        if ((current_time - last_frame_time_usec) < (1000000 / target_fps)) {
            return;
        }
        last_frame_time_usec = current_time;
    }

    g_window_mutex.lock();
    void* current_window = g_second_window;
    g_window_mutex.unlock();

    if (!current_window) {
        if (initialized) _cleanup_vulkan();
        return;
    }

    if (initialized && current_window != last_window) {
        _cleanup_vulkan();
    }

    if (!initialized) {
        _init_vulkan();
        if (!initialized) return;
    }

    vkWaitForFences(vk_device, 1, &in_flight_fence, VK_TRUE, UINT64_MAX);

    RenderingServer* rs = RenderingServer::get_singleton();
    RenderingDevice* rd = rs->get_rendering_device();
    RID rd_texture_rid = rs->texture_get_rd_texture(texture_rid);

    if (!rd_texture_rid.is_valid()) return;

    VkImage source_image = (VkImage)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_VULKAN_IMAGE, rd_texture_rid, 0);
    if (!source_image) return;

    uint32_t imageIndex;
    VkResult res = vkAcquireNextImageKHR(vk_device, swapchain, UINT64_MAX, image_available_semaphore, VK_NULL_HANDLE, &imageIndex);

    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_ERROR_SURFACE_LOST_KHR) {
        _cleanup_vulkan();
        return;
    } else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        return;
    }

    vkResetFences(vk_device, 1, &in_flight_fence);
    vkResetCommandBuffer(command_buffer, 0);
    
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(command_buffer, &beginInfo);

    VkImageMemoryBarrier barrier_dst = {};
    barrier_dst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier_dst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier_dst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier_dst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier_dst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier_dst.image = swapchain_images[imageIndex];
    barrier_dst.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier_dst.subresourceRange.levelCount = 1;
    barrier_dst.subresourceRange.layerCount = 1;
    barrier_dst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    VkImageMemoryBarrier barrier_src = {};
    barrier_src.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier_src.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier_src.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier_src.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier_src.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier_src.image = source_image;
    barrier_src.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier_src.subresourceRange.levelCount = 1;
    barrier_src.subresourceRange.layerCount = 1;
    barrier_src.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier_src.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    Ref<RDTextureFormat> texture_format = rd->texture_get_format(rd_texture_rid);
    if (texture_format.is_null()) return;

    int32_t src_w = (int32_t)texture_format->get_width();
    int32_t src_h = (int32_t)texture_format->get_height();
    if (src_w <= 0 || src_h <= 0) return;

    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier_dst);
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier_src);

    VkImageBlit blit = {};
    blit.srcOffsets[0] = {src_w, src_h, 0};
    blit.srcOffsets[1] = {0, 0, 1};
    blit.dstOffsets[0] = {0, 0, 0};
    blit.dstOffsets[1] = {(int32_t)width, (int32_t)height, 1};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.layerCount = 1;
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.layerCount = 1;

    vkCmdBlitImage(command_buffer, source_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapchain_images[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

    VkImageMemoryBarrier barrier_present = {};
    barrier_present.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier_present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier_present.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier_present.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier_present.image = swapchain_images[imageIndex];
    barrier_present.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier_present.subresourceRange.levelCount = 1;
    barrier_present.subresourceRange.layerCount = 1;
    barrier_present.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    VkImageMemoryBarrier barrier_restore = {};
    barrier_restore.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier_restore.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier_restore.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier_restore.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier_restore.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier_restore.image = source_image;
    barrier_restore.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier_restore.subresourceRange.levelCount = 1;
    barrier_restore.subresourceRange.layerCount = 1;
    barrier_restore.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier_restore.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier_present);
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier_restore);

    vkEndCommandBuffer(command_buffer);

    VkSemaphore waitSemaphores[] = {image_available_semaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signalSemaphores[] = {render_finished_semaphore};

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &command_buffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(vk_queue, 1, &submitInfo, in_flight_fence) != VK_SUCCESS) {
        return;
    }

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &imageIndex;

    res = vkQueuePresentKHR(vk_queue, &presentInfo);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_ERROR_SURFACE_LOST_KHR) {
        _cleanup_vulkan();
    }
#endif
}

void AynThorRenderer::_cleanup_vulkan() {
#ifdef __ANDROID__
    if (initialized && vk_device) {
        vkDeviceWaitIdle(vk_device);
        if (image_available_semaphore) {
            vkDestroySemaphore(vk_device, image_available_semaphore, nullptr);
            image_available_semaphore = VK_NULL_HANDLE;
        }
        if (render_finished_semaphore) {
            vkDestroySemaphore(vk_device, render_finished_semaphore, nullptr);
            render_finished_semaphore = VK_NULL_HANDLE;
        }
        if (in_flight_fence) {
            vkDestroyFence(vk_device, in_flight_fence, nullptr);
            in_flight_fence = VK_NULL_HANDLE;
        }
        if (swapchain) {
            vkDestroySwapchainKHR(vk_device, swapchain, nullptr);
            swapchain = VK_NULL_HANDLE;
        }
        swapchain_images.clear();
        if (surface) {
            vkDestroySurfaceKHR(vk_instance, surface, nullptr);
            surface = VK_NULL_HANDLE;
        }
        if (command_pool) {
            vkDestroyCommandPool(vk_device, command_pool, nullptr);
            command_pool = VK_NULL_HANDLE;
        }
        command_buffer = VK_NULL_HANDLE;
    }
    last_window = nullptr;
#endif
    initialized = false;
}

void AynThorRenderer::fill_color(float r, float g, float b) {}

}