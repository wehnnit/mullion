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

#pragma once

#include "Metal.hpp"
#include <cstdlib>
#include <cstring>
#include "mullion_buffer.hpp"
#include "mullion_command_constants.hpp"
#include "mullion_texture.hpp"
#include "log/log.hpp"

namespace mullion {

template <typename Context> struct SimpleCommandContext {
  Context &ctx;

  WMT::Reference<WMT::ComputePipelineState> getComputePipeline(std::string name);
  void startComputePass();
  void endPass();
  void startBlitPass();
  void setComputeBuffer(uint32_t index, WMT::Buffer buffer, uint64_t offset);
  void setComputePSO(WMT::ComputePipelineState pso, WMTSize tgsize);
  void dispatch(WMTSize size);
  void setComputeTexture(uint32_t index, const Rc<Texture> &texture, uint64_t viewId, int flags);
  // binds the window of the view that holds texel `first`, which becomes that texel's index in it
  void setComputeTexelBuffer(uint32_t index, const Rc<Buffer> &buffer, uint64_t viewId, int flags, uint32_t &first);
  void setComputeBuffer(uint32_t index, const Rc<Buffer> &buffer, uint32_t offset, uint32_t length, int flags);
  void *setComputeBytes(uint32_t index, uint32_t length);

  WMT::Library getDefaultLibrary();

  void startRenderPass();
  /**
  All attachements (including depth-stencil) must have the same array length and sample count
  There must be no 'holes' (if index 2 is set, 0 and 1 must be also set)
  */
  void setColorAttachment(uint32_t index, const Rc<Texture> &texture, uint64_t viewId, uint32_t depth_plane);
  void setDepthStencilAttachment(const Rc<Texture> &texture, uint64_t viewId, uint32_t dsv_flag);
  void setRenderPSO(WMT::RenderPipelineState pso);
  void setViewport(WMTViewport viewport);
  void setDepthStencilState(WMT::DepthStencilState dsso);
  void setScissorRect(WMTScissorRect rect);
  void setStencilReference(uint8_t stencil_ref);
  void setFragmentTexture(uint32_t index, const Rc<Texture> &texture, uint64_t viewId);
  void *setFragmentBytes(uint32_t index, uint32_t length);
  void draw(
      WMTPrimitiveType primitive, uint32_t vertex_start, uint32_t vertex_count, int32_t base_instance,
      uint32_t instance_count
  );
};

template <typename Context> class RGB32Copy {
public:
  RGB32Copy(Context &ctx) : ctx_(ctx) {}

  void run(bool expand, WMT::Buffer src, uint64_t src_offset, WMT::Buffer dst, uint64_t dst_offset,
           RGB32CopyLayout layout, WMTSize size) {
    auto &pso = pipelines_[expand];
    if (!pso) {
      pso = ctx_.getComputePipeline(expand ? "cs_expand_rgb32" : "cs_pack_rgb32");
      if (!pso) {
        ERR("Failed to create RGB32 transfer pipeline");
        abort();
      }
    }
    ctx_.endPass();
    ctx_.startComputePass();
    ctx_.setComputePSO(pso, {pso.threadExecutionWidth(), 1, 1});
    ctx_.setComputeBuffer(0, src, src_offset);
    ctx_.setComputeBuffer(1, dst, dst_offset);
    memcpy(ctx_.setComputeBytes(2, sizeof(layout)), &layout, sizeof(layout));
    ctx_.dispatch(size);
    ctx_.endPass();
    ctx_.startBlitPass();
  }

private:
  SimpleCommandContext<Context> ctx_;
  WMT::Reference<WMT::ComputePipelineState> pipelines_[2];
};

// a footprint's raw BC4/BC5 SNORM blocks decode into the float texels the decoded storage holds, the
// same decode an initial-data upload runs on the CPU (mullion_resource_initializer.cpp)
template <typename Context> class BCSnormDecode {
public:
  BCSnormDecode(Context &ctx) : ctx_(ctx) {}

  void run(WMT::Buffer src, uint64_t src_offset, WMT::Buffer dst, uint64_t dst_offset, BCSnormDecodeLayout layout,
           WMTSize size) {
    auto &pso = pipelines_[layout.rg ? 1 : 0];
    if (!pso) {
      pso = ctx_.getComputePipeline("cs_decode_bc_snorm");
      if (!pso) {
        ERR("Failed to create the BC SNORM decode pipeline");
        abort();
      }
    }
    ctx_.endPass();
    ctx_.startComputePass();
    ctx_.setComputePSO(pso, {pso.threadExecutionWidth(), 1, 1});
    ctx_.setComputeBuffer(0, src, src_offset);
    ctx_.setComputeBuffer(1, dst, dst_offset);
    memcpy(ctx_.setComputeBytes(2, sizeof(layout)), &layout, sizeof(layout));
    ctx_.dispatch(size);
    ctx_.endPass();
    ctx_.startBlitPass();
  }

private:
  SimpleCommandContext<Context> ctx_;
  WMT::Reference<WMT::ComputePipelineState> pipelines_[2];
};

// the raw block-compressed storage decodes into the SNORM companion's float texels: the copy that wrote
// the blocks records this right after itself, and the companion is then what the SNORM views read
template <typename Context> class BCSnormCompanionDecode {
public:
  BCSnormCompanionDecode(Context &ctx) : ctx_(ctx) {}

  void run(const Rc<Texture> &raw, uint64_t raw_view, const Rc<Texture> &companion, uint64_t companion_view,
           uint32_t width, uint32_t height, bool rg) {
    auto &pso = pipelines_[rg ? 1 : 0];
    if (!pso) {
      pso = ctx_.getComputePipeline("cs_decode_bc_snorm_texture");
      if (!pso) {
        ERR("Failed to create the BC SNORM companion decode pipeline");
        abort();
      }
    }
    ctx_.endPass();
    ctx_.startComputePass();
    ctx_.setComputePSO(pso, {pso.threadExecutionWidth(), 1, 1});
    ctx_.setComputeTexture(0, raw, raw_view, 0);
    ctx_.setComputeTexture(1, companion, companion_view, 0);
    uint32_t params[4] = {width, height, rg ? 1u : 0u, 0u};
    memcpy(ctx_.setComputeBytes(2, sizeof(params)), params, sizeof(params));
    ctx_.dispatch({width, height, 1});
    ctx_.endPass();
    ctx_.startBlitPass();
  }

private:
  SimpleCommandContext<Context> ctx_;
  WMT::Reference<WMT::ComputePipelineState> pipelines_[2];
};

} // namespace mullion