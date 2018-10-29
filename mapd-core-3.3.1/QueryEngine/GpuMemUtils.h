/*
 * Copyright 2017 MapD Technologies, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef QUERYENGINE_GPUMEMUTILS_H
#define QUERYENGINE_GPUMEMUTILS_H

#include "CompilationOptions.h"
#include "ThrustAllocator.h"

namespace QueryRenderer {
typedef void QueryRenderManager;
typedef void QueryDataLayout;
}  // namespace QueryRenderer

#ifdef HAVE_CUDA
#include <cuda.h>
#else
#include "../Shared/nocuda.h"
#endif  // HAVE_CUDA

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace CudaMgr_Namespace {

class CudaMgr;

}  // CudaMgr_Namespace

namespace Data_Namespace {

class AbstractBuffer;
class DataMgr;

}  // Data_Namespace

class OutOfRenderMemory : public std::runtime_error {
 public:
  OutOfRenderMemory() : std::runtime_error("OutOfMemory") {}
};

class RenderAllocator {
 public:
  RenderAllocator(int8_t* preallocated_ptr,
                  const size_t preallocated_size,
                  const unsigned block_size_x,
                  const unsigned grid_size_x);

  CUdeviceptr alloc(const size_t bytes) {
    auto ptr = preallocated_ptr_ + crt_allocated_bytes_;
    crt_allocated_bytes_ += bytes;
    if (crt_allocated_bytes_ <= preallocated_size_) {
      return reinterpret_cast<CUdeviceptr>(ptr);
    }

    // reset the current allocated bytes for a proper
    // error resolution
    crt_allocated_bytes_ = 0;
    throw OutOfRenderMemory();
  }

  void markChunkComplete() { crt_chunk_offset_bytes_ = crt_allocated_bytes_; }

  size_t getCurrentChunkOffset() const { return crt_chunk_offset_bytes_; }
  size_t getCurrentChunkSize() const { return crt_allocated_bytes_ - crt_chunk_offset_bytes_; }
  size_t getAllocatedSize() const { return crt_allocated_bytes_; }

  int8_t* getBasePtr() const { return preallocated_ptr_; }

 private:
  int8_t* preallocated_ptr_;
  const size_t preallocated_size_;
  size_t crt_chunk_offset_bytes_;
  size_t crt_allocated_bytes_;
};

class RenderAllocatorMap {
 public:
  RenderAllocatorMap(::CudaMgr_Namespace::CudaMgr* cuda_mgr,
                     ::QueryRenderer::QueryRenderManager* render_manager,
                     const unsigned block_size_x,
                     const unsigned grid_size_x);
  ~RenderAllocatorMap();

  RenderAllocator* getRenderAllocator(size_t device_id);
  RenderAllocator* operator[](size_t device_id);

  void setDataLayout(const std::shared_ptr<::QueryRenderer::QueryDataLayout>& query_data_layout);
  void prepForRendering(const std::shared_ptr<::QueryRenderer::QueryDataLayout>& query_data_layout);

 private:
  ::CudaMgr_Namespace::CudaMgr* cuda_mgr_;
  ::QueryRenderer::QueryRenderManager* render_manager_;
  std::vector<RenderAllocator> render_allocator_map_;
};

CUdeviceptr alloc_gpu_mem(Data_Namespace::DataMgr* data_mgr,
                          const size_t num_bytes,
                          const int device_id,
                          RenderAllocator* render_allocator);

Data_Namespace::AbstractBuffer* alloc_gpu_abstract_buffer(Data_Namespace::DataMgr* data_mgr,
                                                          const size_t num_bytes,
                                                          const int device_id);

void free_gpu_abstract_buffer(Data_Namespace::DataMgr* data_mgr, Data_Namespace::AbstractBuffer* ab);

void copy_to_gpu(Data_Namespace::DataMgr* data_mgr,
                 CUdeviceptr dst,
                 const void* src,
                 const size_t num_bytes,
                 const int device_id);

void copy_from_gpu(Data_Namespace::DataMgr* data_mgr,
                   void* dst,
                   const CUdeviceptr src,
                   const size_t num_bytes,
                   const int device_id);

struct GpuQueryMemory {
  std::pair<CUdeviceptr, CUdeviceptr> group_by_buffers;
  std::pair<CUdeviceptr, CUdeviceptr> small_group_by_buffers;
};

struct QueryMemoryDescriptor;

GpuQueryMemory create_dev_group_by_buffers(Data_Namespace::DataMgr* data_mgr,
                                           const std::vector<int64_t*>& group_by_buffers,
                                           const std::vector<int64_t*>& small_group_by_buffers,
                                           const QueryMemoryDescriptor&,
                                           const unsigned block_size_x,
                                           const unsigned grid_size_x,
                                           const int device_id,
                                           const bool prepend_index_buffer,
                                           const bool always_init_group_by_on_host,
                                           RenderAllocator* render_allocator);

void copy_group_by_buffers_from_gpu(Data_Namespace::DataMgr* data_mgr,
                                    const std::vector<int64_t*>& group_by_buffers,
                                    const size_t groups_buffer_size,
                                    const CUdeviceptr group_by_dev_buffers_mem,
                                    const QueryMemoryDescriptor& query_mem_desc,
                                    const unsigned block_size_x,
                                    const unsigned grid_size_x,
                                    const int device_id,
                                    const bool prepend_index_buffer);

class QueryExecutionContext;
struct RelAlgExecutionUnit;

void copy_group_by_buffers_from_gpu(Data_Namespace::DataMgr* data_mgr,
                                    const QueryExecutionContext*,
                                    const GpuQueryMemory&,
                                    const RelAlgExecutionUnit&,
                                    const unsigned block_size_x,
                                    const unsigned grid_size_x,
                                    const int device_id,
                                    const bool prepend_index_buffer);

// TODO(alex): remove
bool buffer_not_null(const QueryMemoryDescriptor& query_mem_desc,
                     const unsigned block_size_x,
                     const ExecutorDeviceType device_type,
                     size_t i);

#endif  // QUERYENGINE_GPUMEMUTILS_H
