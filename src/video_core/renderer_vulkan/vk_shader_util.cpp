// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>
#include <mutex>
#include <unordered_map>

#include "common/cityhash.h"

#include "common/common_types.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

namespace {
std::mutex module_hashes_mutex;
std::unordered_map<VkShaderModule, u64> module_hashes;
} // Anonymous namespace

u64 ShaderModuleCodeHash(VkShaderModule module) {
    std::scoped_lock lock{module_hashes_mutex};
    const auto it = module_hashes.find(module);
    return it == module_hashes.end() ? 0 : it->second;
}

vk::ShaderModule BuildShader(const Device& device, std::span<const u32> code) {
    vk::ShaderModule module = device.GetLogical().CreateShaderModule({
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = static_cast<u32>(code.size_bytes()),
        .pCode = code.data(),
    });
    // Pipeline feedback diagnostics compare the SPIR-V a prepared and a drawn pipeline use.
    const u64 hash = Common::CityHash64(reinterpret_cast<const char*>(code.data()), code.size_bytes());
    {
        std::scoped_lock lock{module_hashes_mutex};
        module_hashes.insert_or_assign(*module, hash);
    }
    return module;
}

} // namespace Vulkan
