/*
 * Copyright 2026 Feifan He for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "Metal.hpp"
#include "mullion_resource_initializer.hpp"
#include "mullion_format.hpp"
#include "util_math.hpp"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

namespace mullion {

#define ALLOC_BLIT(type, cmd)                                                                                          \
  type *cmd = nullptr;                                                                                                 \
  if (!allocateBlit(&cmd)) {                                                                                           \
    flushInternal();                                                                                                   \
    continue;                                                                                                          \
  }

#define ALLOC_CLEAR(info)                                                                                              \
  WMTRenderPassInfo *info;                                                                                             \
  if (!allocateClear(&info)) {                                                                                         \
    flushInternal();                                                                                                   \
    continue;                                                                                                          \
  }

#define ALLOC_GPU(buffer, size)                                                                                        \
  WMT::Buffer buffer;                                                                                                  \
  size_t buffer##_offset;                                                                                              \
  if (!(buffer = allocateGpuHeap(size, buffer##_offset))) {                                                            \
    flushInternal();                                                                                                   \
    continue;                                                                                                          \
  }

#define ALLOC_ZERO(buffer, size)                                                                                       \
  WMT::Buffer buffer;                                                                                                  \
  if (!(buffer = allocateZeroBuffer(size))) {                                                                          \
    flushInternal();                                                                                                   \
    continue;                                                                                                          \
  }

#define RETAIN(allocation)                                                                                             \
  if (!retainAllocation(allocation)) {                                                                                 \
    flushInternal();                                                                                                   \
    continue;                                                                                                          \
  }

ResourceInitializer::ResourceInitializer(WMT::Device device) :
    device_(device),
    gpu_command_heap_allocator(StagingBufferBlockAllocator(
        device, WMTResourceStorageModeManaged | WMTResourceHazardTrackingModeUntracked, false
    )) {
  upload_queue_ = device.newCommandQueue(kResourceInitializerChunks);
  upload_queue_event_ = device.newSharedEvent();

  cpu_command_heap_size = kResourceInitializerCpuCommandHeapSize;
  cpu_command_heap = malloc(cpu_command_heap_size);
  reset();
}

ResourceInitializer::~ResourceInitializer() {
  free(cpu_command_heap);
}

uint64_t
ResourceInitializer::initWithZero(BufferAllocation *buffer, uint64_t offset, uint64_t length) {
  std::lock_guard<mullion::mutex> lock(mutex_);

  do {
    RETAIN(buffer);
    ALLOC_BLIT(wmtcmd_blit_fillbuffer, fill);
    fill->type = WMTBlitCommandFillBuffer;
    fill->buffer = buffer->buffer();
    fill->offset = offset;
    fill->length = length;
    fill->value = 0;

  } while (0);

  return current_seq_id_;
}

uint64_t
ResourceInitializer::initDepthStencilWithZero(
    const Texture *texture, TextureAllocation *allocation, uint32_t slice, uint32_t level, uint32_t dsv_planar
) {
  auto width_sub = std::max(1u, texture->width() >> level);
  auto height_sub = std::max(1u, texture->height() >> level);

  std::lock_guard<mullion::mutex> lock(mutex_);
  do {
    RETAIN(allocation);
    ALLOC_CLEAR(info);

    info->render_target_array_length = 0;
    info->render_target_width = width_sub;
    info->render_target_height = height_sub;
    if (dsv_planar & 1) {
      info->depth.texture = allocation->texture();
      info->depth.clear_depth = 0;
      info->depth.load_action = WMTLoadActionClear;
      info->depth.store_action = WMTStoreActionStore;
      info->depth.slice = slice;
      info->depth.level = level;
    }
    if (dsv_planar & 2) {
      info->stencil.texture = allocation->texture();
      info->stencil.clear_stencil = 0;
      info->stencil.load_action = WMTLoadActionClear;
      info->stencil.store_action = WMTStoreActionStore;
      info->stencil.slice = slice;
      info->stencil.level = level;
    }

  } while (0);

  return current_seq_id_;
}

uint64_t
ResourceInitializer::initRenderTargetWithZero(
    const Texture *texture, TextureAllocation *allocation, uint32_t slice, uint32_t level
) {
  auto width_sub = std::max(1u, texture->width() >> level);
  auto height_sub = std::max(1u, texture->height() >> level);

  std::lock_guard<mullion::mutex> lock(mutex_);
  do {
    RETAIN(allocation);
    ALLOC_CLEAR(info);

    info->render_target_array_length = texture->textureType() == WMTTextureType3D ? (texture->depth() >> level) : 0;
    info->render_target_width = width_sub;
    info->render_target_height = height_sub;
    info->colors[0].texture = allocation->texture();
    info->colors[0].load_action = WMTLoadActionClear;
    info->colors[0].clear_color = {0, 0, 0, 0};
    info->colors[0].store_action = WMTStoreActionStore;
    info->colors[0].slice = slice;
    info->colors[0].level = level;

  } while (0);

  return current_seq_id_;
}

uint64_t
ResourceInitializer::initWithZero(
    const Texture *texture, TextureAllocation *allocation, uint32_t slice, uint32_t level
) {
  if (auto dsv_planar = DepthStencilPlanarFlags(texture->pixelFormat())) {
    return initDepthStencilWithZero(texture, allocation, slice, level, dsv_planar);
  }
  if (texture->usage() & WMTTextureUsageRenderTarget) {
    return initRenderTargetWithZero(texture, allocation, slice, level);
  }

  auto width_sub = std::max(1u, texture->width() >> level);
  auto height_sub = std::max(1u, texture->height() >> level);
  auto depth_sub = std::max(1u, texture->depth() >> level);

  auto block_size = 1u;

  switch (texture->pixelFormat()) {
  case WMTPixelFormatBC1_RGBA:
  case WMTPixelFormatBC1_RGBA_sRGB:
  case WMTPixelFormatBC2_RGBA:
  case WMTPixelFormatBC2_RGBA_sRGB:
  case WMTPixelFormatBC3_RGBA:
  case WMTPixelFormatBC3_RGBA_sRGB:
  case WMTPixelFormatBC4_RSnorm:
  case WMTPixelFormatBC4_RUnorm:
  case WMTPixelFormatBC5_RGUnorm:
  case WMTPixelFormatBC5_RGSnorm:
  case WMTPixelFormatBC6H_RGBUfloat:
  case WMTPixelFormatBC6H_RGBFloat:
  case WMTPixelFormatBC7_RGBAUnorm:
  case WMTPixelFormatBC7_RGBAUnorm_sRGB:
    block_size = 4u;
    break;
  default:
    break;
  }

  bool is_3d_tex = texture->textureType() == WMTTextureType3D;
  // a multisampled texel is its samples
  size_t texel_size = MTLGetTexelSize(texture->pixelFormat()) * texture->sampleCount();
  size_t bytes_per_row_needed = texel_size * align(width_sub, block_size) / block_size;
  size_t bytes_per_image_needed = bytes_per_row_needed * align(height_sub, block_size) / block_size;
  size_t total_bytes_needed = bytes_per_image_needed * depth_sub;

  std::lock_guard<mullion::mutex> lock(mutex_);

  do {
    ALLOC_ZERO(zero, total_bytes_needed);
    RETAIN(allocation);
    ALLOC_BLIT(wmtcmd_blit_copy_from_buffer_to_texture, copy);

    copy->type = WMTBlitCommandCopyFromBufferToTexture;
    copy->src = zero;
    copy->src_offset = 0;
    copy->bytes_per_row = bytes_per_row_needed;
    copy->bytes_per_image = is_3d_tex ? bytes_per_image_needed : 0;
    copy->size = {width_sub, height_sub, depth_sub};
    copy->dst = allocation->texture();
    copy->slice = slice;
    copy->level = level;
    copy->origin = {0, 0, 0};

  } while (0);

  return current_seq_id_;
}

// D3D11.3 19.5.6/.7. Apple's BC4/BC5 signed decoder is not exact at 0, so the texels are stored decoded.
static double
bc4_value(const uint8_t *block, unsigned texel, bool signed_values) {
  int e0 = block[0], e1 = block[1];
  if (signed_values) {
    if (e0 > 127)
      e0 -= 256;
    if (e1 > 127)
      e1 -= 256;
  }
  uint64_t bits = 0;
  for (unsigned i = 0; i < 6; i++)
    bits |= uint64_t(block[2 + i]) << (8 * i);
  unsigned index = (bits >> (texel * 3)) & 7;
  double a = signed_values ? std::max(-1.0, e0 / 127.0) : e0 / 255.0;
  double b = signed_values ? std::max(-1.0, e1 / 127.0) : e1 / 255.0;
  if (index == 0)
    return a;
  if (index == 1)
    return b;
  if (e0 > e1)
    return ((8 - int(index)) * a + (int(index) - 1) * b) / 7;
  if (index < 6)
    return ((6 - int(index)) * a + (int(index) - 1) * b) / 5;
  return index == 6 ? (signed_values ? -1.0 : 0.0) : 1.0;
}
static std::vector<uint8_t>
decode_bc_snorm(const void *blocks, uint32_t width, uint32_t height, size_t row_pitch, bool rg) {
  unsigned channels = rg ? 2u : 1u;
  unsigned block_bytes = channels * 8;
  std::vector<uint8_t> out(size_t(width) * height * channels * sizeof(float));
  auto *dst = reinterpret_cast<float *>(out.data());
  for (uint32_t y = 0; y < height; y++)
    for (uint32_t x = 0; x < width; x++) {
      auto *block = static_cast<const uint8_t *>(ptr_add(blocks, (y / 4) * row_pitch + (x / 4) * block_bytes));
      unsigned texel = (y % 4) * 4 + (x % 4);
      for (unsigned c = 0; c < channels; c++)
        dst[(size_t(y) * width + x) * channels + c] = float(bc4_value(block + c * 8, texel, true));
    }
  return out;
}

uint64_t
ResourceInitializer::initWithData(
    const Texture *texture, TextureAllocation *allocation, uint32_t slice, uint32_t level, const void *data,
    size_t row_pitch, size_t depth_pitch, uint32_t format_flags
) {
  auto width_sub = std::max(1u, texture->width() >> level);
  auto height_sub = std::max(1u, texture->height() >> level);
  auto depth_sub = std::max(1u, texture->depth() >> level);

  std::vector<uint8_t> decoded;
  if (format_flags & MTL_DXGI_FORMAT_BC_SNORM_DECODED) {
    bool rg = texture->pixelFormat() == WMTPixelFormatRG32Float;
    decoded = decode_bc_snorm(data, width_sub, height_sub, row_pitch, rg);
    data = decoded.data();
    row_pitch = size_t(width_sub) * (rg ? 8u : 4u);
    depth_pitch = row_pitch * height_sub;
  } else if (texture->pixelFormat() == WMTPixelFormatBC4_RUnorm || texture->pixelFormat() == WMTPixelFormatBC5_RGUnorm) {
    // a signed view of these blocks is decoded from the copy kept here (19.5.2)
    size_t channels = texture->pixelFormat() == WMTPixelFormatBC5_RGUnorm ? 2 : 1;
    size_t blocks_x = (width_sub + 3) / 4, blocks_y = (height_sub + 3) / 4, block_bytes = channels * 8;
    auto &mips = const_cast<Texture *>(texture)->bc_mips;
    if (mips.size() <= level)
      mips.resize(level + 1);
    auto &stored = mips[level];
    stored.resize(blocks_x * blocks_y * block_bytes);
    for (size_t row = 0; row < blocks_y; row++)
      memcpy(stored.data() + row * blocks_x * block_bytes, ptr_add(data, row * row_pitch), blocks_x * block_bytes);
  }

  auto block_size = 1u;

  switch (texture->pixelFormat()) {
  case WMTPixelFormatBC1_RGBA:
  case WMTPixelFormatBC1_RGBA_sRGB:
  case WMTPixelFormatBC2_RGBA:
  case WMTPixelFormatBC2_RGBA_sRGB:
  case WMTPixelFormatBC3_RGBA:
  case WMTPixelFormatBC3_RGBA_sRGB:
  case WMTPixelFormatBC4_RSnorm:
  case WMTPixelFormatBC4_RUnorm:
  case WMTPixelFormatBC5_RGUnorm:
  case WMTPixelFormatBC5_RGSnorm:
  case WMTPixelFormatBC6H_RGBUfloat:
  case WMTPixelFormatBC6H_RGBFloat:
  case WMTPixelFormatBC7_RGBAUnorm:
  case WMTPixelFormatBC7_RGBAUnorm_sRGB:
    block_size = 4u;
    break;
  case WMTPixelFormatDepth32Float_Stencil8:
    return initDepthStencilWithData(
        texture, allocation, slice, level, data, row_pitch, (format_flags & MTL_DXGI_FORMAT_EMULATED_D24)
    );
  default:
    break;
  }

  bool is_1d_tex = (texture->textureType() == WMTTextureType1D) || (texture->textureType() == WMTTextureType1DArray);
  bool is_3d_tex = texture->textureType() == WMTTextureType3D;
  size_t texel_size = MTLGetTexelSize(texture->pixelFormat());
  size_t bytes_per_row_needed = texel_size * align(width_sub, block_size) / block_size;
  size_t bytes_per_row_increment = is_1d_tex ? bytes_per_row_needed : row_pitch;
  size_t bytes_per_row_valid = is_1d_tex ? bytes_per_row_needed : std::min(row_pitch, bytes_per_row_needed);
  size_t bytes_per_image_needed = bytes_per_row_needed * align(height_sub, block_size) / block_size;
  size_t bytes_per_image_increment = is_3d_tex ? depth_pitch : bytes_per_image_needed;
  size_t bytes_per_image_valid = is_3d_tex ? std::min(depth_pitch, bytes_per_image_needed) : bytes_per_image_needed;
  size_t total_bytes_needed = bytes_per_image_needed * depth_sub;

  std::lock_guard<mullion::mutex> lock(mutex_);
  do {
    RETAIN(allocation);
    ALLOC_BLIT(wmtcmd_blit_copy_from_buffer_to_texture, copy);
    ALLOC_GPU(temp, total_bytes_needed);

    std::vector<uint8_t> rgba;
    if (format_flags & MTL_DXGI_FORMAT_EMULATED_RGB32)
      rgba.resize(bytes_per_row_needed);
    for (auto depth = 0u; depth < depth_sub; depth++) {
      if (format_flags & MTL_DXGI_FORMAT_EMULATED_RGB32) {
        for (auto row = 0u; row < height_sub; row++) {
          auto src = ptr_add(data, depth * depth_pitch + row * row_pitch);
          CopyRGB32(rgba.data(), src, width_sub, true);
          temp.updateContents(temp_offset + depth * bytes_per_image_needed + row * bytes_per_row_needed,
                              rgba.data(), bytes_per_row_needed);
        }
      } else if (bytes_per_row_increment != bytes_per_row_needed) {
        for (auto row = 0u; row < (align(height_sub, block_size) / block_size); row++) {
          auto offset = temp_offset + depth * bytes_per_image_needed + row * bytes_per_row_needed;
          auto length = bytes_per_row_valid;
          auto src_data = ptr_add(data, depth * bytes_per_image_increment + row * bytes_per_row_increment);
          temp.updateContents(offset, src_data, length);
        }
      } else {
        auto offset = temp_offset + depth * bytes_per_image_needed;
        auto length = bytes_per_image_valid;
        auto src_data = ptr_add(data, depth * bytes_per_image_increment);
        temp.updateContents(offset, src_data, length);
      }
    }

    copy->type = WMTBlitCommandCopyFromBufferToTexture;
    copy->src = temp;
    copy->src_offset = temp_offset;
    copy->bytes_per_row = bytes_per_row_needed;
    copy->bytes_per_image = is_3d_tex ? bytes_per_image_needed : 0;
    copy->size = {width_sub, height_sub, depth_sub};
    copy->dst = allocation->texture();
    copy->slice = slice;
    copy->level = level;
    copy->origin = {0, 0, 0};

  } while (0);

  return current_seq_id_;
}

uint64_t
ResourceInitializer::initDepthStencilWithData(
    const Texture *texture, TextureAllocation *allocation, uint32_t slice, uint32_t level, const void *data,
    size_t row_pitch, bool is_d24s8
) {
  auto width_sub = std::max(1u, texture->width() >> level);
  auto height_sub = std::max(1u, texture->height() >> level);

  size_t depth_bytes_per_row = 4 * width_sub;
  size_t stencil_bytes_per_row = align(width_sub, 4u);

  size_t depth_bytes_per_image = depth_bytes_per_row * height_sub;
  size_t stencil_bytes_per_image = stencil_bytes_per_row * height_sub;

  std::lock_guard<mullion::mutex> lock(mutex_);
  do {
    RETAIN(allocation);
    ALLOC_BLIT(wmtcmd_blit_copy_from_buffer_to_texture_withblitoption, depth_copy);
    ALLOC_BLIT(wmtcmd_blit_copy_from_buffer_to_texture_withblitoption, stencil_copy);
    ALLOC_GPU(depth_buf, depth_bytes_per_image);
    ALLOC_GPU(stencil_buf, stencil_bytes_per_image);

    for (auto row = 0u; row < height_sub; row++) {
      auto src_data = ptr_add(data, row * row_pitch);

      if (is_d24s8) {
        for (auto x = 0u; x < width_sub; x++) {
          uint32_t packed = *(const uint32_t *)((const char *)src_data + x * 4);
          float depth_val = (float)(packed & 0xFFFFFF) / (float)(0xFFFFFF);
          uint8_t stencil_val = (uint8_t)(packed >> 24);
          depth_buf.updateContents(depth_buf_offset + row * depth_bytes_per_row + x * 4, &depth_val, 4);
          stencil_buf.updateContents(stencil_buf_offset + row * stencil_bytes_per_row + x, &stencil_val, 1);
        }
      } else {
        for (auto x = 0u; x < width_sub; x++) {
          depth_buf.updateContents(depth_buf_offset + row * depth_bytes_per_row + x * 4, (const char *)src_data + x * 8, 4);
          stencil_buf.updateContents(stencil_buf_offset + row * stencil_bytes_per_row + x, (const uint8_t *)src_data + x * 8 + 4, 1);
        }
      }
    }

    depth_copy->type = WMTBlitCommandCopyFromBufferToTextureWithBlitOption;
    depth_copy->src = depth_buf;
    depth_copy->src_offset = depth_buf_offset;
    depth_copy->bytes_per_row = depth_bytes_per_row;
    depth_copy->bytes_per_image = 0;
    depth_copy->size = {width_sub, height_sub, 1};
    depth_copy->dst = allocation->texture();
    depth_copy->slice = slice;
    depth_copy->level = level;
    depth_copy->origin = {0, 0, 0};
    depth_copy->options = WMTBlitOptionDepthFromDepthStencil;

    stencil_copy->type = WMTBlitCommandCopyFromBufferToTextureWithBlitOption;
    stencil_copy->src = stencil_buf;
    stencil_copy->src_offset = stencil_buf_offset;
    stencil_copy->bytes_per_row = stencil_bytes_per_row;
    stencil_copy->bytes_per_image = 0;
    stencil_copy->size = {width_sub, height_sub, 1};
    stencil_copy->dst = allocation->texture();
    stencil_copy->slice = slice;
    stencil_copy->level = level;
    stencil_copy->origin = {0, 0, 0};
    stencil_copy->options = WMTBlitOptionStencilFromDepthStencil;

  } while (0);

  return current_seq_id_;
}

std::uint64_t
ResourceInitializer::flushInternal() {
  auto pool = WMT::MakeAutoreleasePool();

  auto seq_id = current_seq_id_++;
  auto cmdbuf = upload_queue_.commandBuffer();
  encode(cmdbuf);
  cmdbuf.encodeSignalEvent(upload_queue_event_, seq_id);
  cmdbuf.commit();
  reset();
  cached_coherent_seq_id = upload_queue_event_.signaledValue();
  gpu_command_heap_allocator.free_blocks(cached_coherent_seq_id);
  return seq_id;
}

uint64_t
ResourceInitializer::flushToWait() {
  std::lock_guard<mullion::mutex> lock(mutex_);

  if (idle()) {
    gpu_command_heap_allocator.free_blocks(cached_coherent_seq_id);
    if (cached_coherent_seq_id == current_seq_id_ - 1)
      return 0;
    cached_coherent_seq_id = upload_queue_event_.signaledValue();
    if (cached_coherent_seq_id == current_seq_id_ - 1)
      return 0;
    return current_seq_id_ - 1;
  }

  return flushInternal();
}

void
ResourceInitializer::reset() {
  cpu_command_heap_offset = 0;

  clear_render_pass_head.next = nullptr;
  clear_render_pass_tail = &clear_render_pass_head;

  blit_cmd_head.type = WMTBlitCommandNop;
  blit_cmd_head.next.set(nullptr);
  blit_cmd_tail = (wmtcmd_base *)&blit_cmd_head;

  ref_tracker.clear();
}

void
ResourceInitializer::encode(WMT::CommandBuffer cmdbuf) {

  auto clear_pass = clear_render_pass_head.next;
  while (clear_pass) {
    auto r = cmdbuf.renderCommandEncoder(clear_pass->info);
    r.endEncoding();
    clear_pass = clear_pass->next;
  }

  if (blit_cmd_head.next.ptr) {
    auto b = cmdbuf.blitCommandEncoder();
    b.encodeCommands(&blit_cmd_head);
    b.endEncoding();
  }
}

WMT::Buffer
ResourceInitializer::allocateGpuHeap(size_t size, size_t &offset) {
  auto [block, offset_] = gpu_command_heap_allocator.allocate(
      current_seq_id_, cached_coherent_seq_id, size, kResourceInitializerGpuUploadHeapAlignment
  );
  offset = offset_;
  return block.buffer;
}

bool
ResourceInitializer::retainAllocation(Allocation *allocation) {
  constexpr size_t block_size = 0x20;
  while (unlikely(!ref_tracker.track(allocation))) {
    auto temp = allocateCpuHeap<intptr_t[block_size]>();
    if (!temp)
      return false;
    ref_tracker.addStorage(temp, block_size * sizeof(intptr_t));
  }
  return true;
}

WMT::Buffer
ResourceInitializer::allocateZeroBuffer(size_t size) {
  if (zero_buffer_size_ < size) {
    if (zero_buffer_size_) {
      flushInternal(); // keep a reference of old zero buffer
      zero_buffer_size_ = 0;
      return {};
    }

    wmtcmd_blit_fillbuffer *fill = nullptr;
    if (!allocateBlit(&fill)) {
      return {};
    }

    WMTBufferInfo buffer_info;
    buffer_info.gpu_address = 0;
    buffer_info.length = size;
    buffer_info.memory.set(nullptr);
    buffer_info.options = WMTResourceStorageModePrivate | WMTResourceHazardTrackingModeUntracked;
    zero_buffer_ = device_.newBuffer(buffer_info);
    zero_buffer_size_ = size;

    fill->type = WMTBlitCommandFillBuffer;
    fill->buffer = zero_buffer_;
    fill->length = size;
    fill->offset = 0;
    fill->value = 0;
  }
  return zero_buffer_;
}

} // namespace mullion