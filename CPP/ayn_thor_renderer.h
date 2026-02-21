#ifndef AYN_THOR_RENDERER_H
#define AYN_THOR_RENDERER_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <vector>

#ifdef __ANDROID__
#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan.h>
#include <android/native_window.h>
#endif

namespace godot {

class AynThorRenderer : public Node {
    GDCLASS(AynThorRenderer, Node)

private:
#ifdef __ANDROID__
    VkInstance vk_instance = VK_NULL_HANDLE;
    VkPhysicalDevice vk_physical_device = VK_NULL_HANDLE;
    VkDevice vk_device = VK_NULL_HANDLE;
    VkQueue vk_queue = VK_NULL_HANDLE;
    uint32_t vk_queue_family_index = 0;

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> swapchain_images;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;

    VkSemaphore image_available_semaphore = VK_NULL_HANDLE;
    VkSemaphore render_finished_semaphore = VK_NULL_HANDLE;
    VkFence in_flight_fence = VK_NULL_HANDLE;

    VkFormat swapchain_image_format = VK_FORMAT_R8G8B8A8_UNORM;
#endif

    uint32_t width = 0;
    uint32_t height = 0;

    bool initialized = false;
    void* last_window = nullptr;
    
    int target_fps = 0;
    uint64_t last_frame_time_usec = 0;
    int rotation_degrees = 180;

    void _init_vulkan();
    void _cleanup_vulkan();
    void _create_swapchain();

protected:
    static void _bind_methods();

public:
    AynThorRenderer();
    ~AynThorRenderer();

    bool is_window_available();
    void fill_color(float r, float g, float b);
    void draw_viewport_texture(RID texture_rid);
    Vector2i get_second_screen_size();

    void set_target_fps(int p_fps);
    int get_target_fps() const;

    void set_rotation_degrees(int p_degrees);
    int get_rotation_degrees() const;
};

}

#endif