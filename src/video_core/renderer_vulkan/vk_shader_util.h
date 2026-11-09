// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>

#include <span>

#include "common/common_types.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

class Device;

vk::ShaderModule BuildShader(const Device& device, std::span<const u32> code);

/// CityHash64 of the SPIR-V a module was built from (0 when unknown), for pipeline diagnostics.
u64 ShaderModuleCodeHash(VkShaderModule module);

/// Above 0 while the native (recipe) preparation builds pipelines: diagnostics tag them "prep".
inline std::atomic<int> g_native_preparing{0};

} // namespace Vulkan
