#include "vulkan_device.hpp"
#include <iostream>
#include <vector>
#include <algorithm>

VulkanDevice::VulkanDevice() {
    available_ = init_vulkan();
    if (available_) {
        std::cout << "[VulkanDevice] Initialized GPU: " << device_name_ 
                  << " | Dedicated VRAM: " << (total_vram_bytes_ / (1024 * 1024)) << " MiB"
                  << " | 85% Safety Ceiling: " << (safe_ceiling_bytes_ / (1024 * 1024)) << " MiB"
                  << std::endl;
    } else {
        std::cout << "[VulkanDevice] GPU compute acceleration unavailable. Fallback to CPU/AVX2." << std::endl;
    }
}

VulkanDevice::~VulkanDevice() {
    cleanup();
}

bool VulkanDevice::init_vulkan() {
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "DenseLite";
    app_info.applicationVersion = VK_MAKE_VERSION(3, 2, 1);
    app_info.pEngineName = "DenseLiteRuntime";
    app_info.engineVersion = VK_MAKE_VERSION(3, 2, 1);
    app_info.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    VkResult res = vkCreateInstance(&create_info, nullptr, &instance_);
    if (res != VK_SUCCESS) {
        return false;
    }

    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance_, &device_count, nullptr);
    if (device_count == 0) {
        return false;
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance_, &device_count, devices.data());

    // Prefer discrete GPU over integrated GPU
    VkPhysicalDevice selected_device = VK_NULL_HANDLE;
    int best_score = -1;

    for (const auto& dev : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);
        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score = 100;
        } else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            score = 50;
        }
        if (score > best_score) {
            best_score = score;
            selected_device = dev;
        }
    }

    if (selected_device == VK_NULL_HANDLE) {
        selected_device = devices[0];
    }

    physical_device_ = selected_device;
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physical_device_, &props);
    device_name_ = props.deviceName;
    device_type_ = props.deviceType;
    limits_ = props.limits;

    vkGetPhysicalDeviceMemoryProperties(physical_device_, &mem_properties_);

    // Compute total dedicated device-local VRAM
    total_vram_bytes_ = 0;
    for (uint32_t i = 0; i < mem_properties_.memoryHeapCount; ++i) {
        if (mem_properties_.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            total_vram_bytes_ += mem_properties_.memoryHeaps[i].size;
        }
    }

    // 85% Safety Ceiling: 15% strictly reserved for display server / compositor
    safe_ceiling_bytes_ = static_cast<size_t>(total_vram_bytes_ * 0.85);

    // Find compute queue family
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, queue_families.data());

    int compute_queue_family = -1;
    for (uint32_t i = 0; i < queue_family_count; ++i) {
        if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            compute_queue_family = static_cast<int>(i);
            break;
        }
    }

    if (compute_queue_family == -1) {
        return false;
    }

    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info{};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = static_cast<uint32_t>(compute_queue_family);
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;

    VkDeviceCreateInfo device_ci{};
    device_ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_ci.queueCreateInfoCount = 1;
    device_ci.pQueueCreateInfos = &queue_create_info;

    res = vkCreateDevice(physical_device_, &device_ci, nullptr, &device_);
    return (res == VK_SUCCESS);
}

void VulkanDevice::cleanup() {
    if (device_ != VK_NULL_HANDLE) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
    available_ = false;
}

VulkanMemoryInfo VulkanDevice::memory_info() const {
    std::lock_guard<std::mutex> lock(alloc_mutex_);
    VulkanMemoryInfo info;
    info.total_vram_bytes = total_vram_bytes_;
    info.safe_ceiling_bytes = safe_ceiling_bytes_;
    info.allocated_bytes = allocated_bytes_;
    info.available_budget_bytes = (allocated_bytes_ < safe_ceiling_bytes_) ? (safe_ceiling_bytes_ - allocated_bytes_) : 0;
    return info;
}

size_t VulkanDevice::safe_ceiling_bytes() const {
    return safe_ceiling_bytes_;
}

size_t VulkanDevice::allocated_bytes() const {
    std::lock_guard<std::mutex> lock(alloc_mutex_);
    return allocated_bytes_;
}

size_t VulkanDevice::available_safe_bytes() const {
    std::lock_guard<std::mutex> lock(alloc_mutex_);
    return (allocated_bytes_ < safe_ceiling_bytes_) ? (safe_ceiling_bytes_ - allocated_bytes_) : 0;
}

bool VulkanDevice::can_admit(size_t bytes) const {
    if (!available_) return false;
    std::lock_guard<std::mutex> lock(alloc_mutex_);
    return (allocated_bytes_ + bytes <= safe_ceiling_bytes_);
}

bool VulkanDevice::allocate(size_t bytes) {
    if (!available_) return false;
    std::lock_guard<std::mutex> lock(alloc_mutex_);
    if (allocated_bytes_ + bytes > safe_ceiling_bytes_) {
        return false;
    }
    allocated_bytes_ += bytes;
    return true;
}

void VulkanDevice::release(size_t bytes) {
    std::lock_guard<std::mutex> lock(alloc_mutex_);
    if (bytes >= allocated_bytes_) {
        allocated_bytes_ = 0;
    } else {
        allocated_bytes_ -= bytes;
    }
}
