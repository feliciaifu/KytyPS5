#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_PIPELINE_PIPELINEBREADCRUMB_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_PIPELINE_PIPELINEBREADCRUMB_H_

#include <atomic>
#include <cstdint>

namespace Libs::Graphics {

struct PipelineBreadcrumb {
	std::atomic<uint32_t> kind {0};
	std::atomic<uint64_t> stage0 {0};
	std::atomic<uint64_t> stage1 {0};
	std::atomic<uint64_t> stage2 {0};
	std::atomic<uint64_t> stage3 {0};
};

inline PipelineBreadcrumb& GetPipelineBreadcrumb() {
	static PipelineBreadcrumb breadcrumb;
	return breadcrumb;
}

inline void SetPipelineBreadcrumb(uint32_t kind, uint64_t stage0, uint64_t stage1 = 0,
                                  uint64_t stage2 = 0, uint64_t stage3 = 0) {
	auto& breadcrumb = GetPipelineBreadcrumb();
	breadcrumb.stage0.store(stage0, std::memory_order_relaxed);
	breadcrumb.stage1.store(stage1, std::memory_order_relaxed);
	breadcrumb.stage2.store(stage2, std::memory_order_relaxed);
	breadcrumb.stage3.store(stage3, std::memory_order_relaxed);
	breadcrumb.kind.store(kind, std::memory_order_relaxed);
}

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_PIPELINE_PIPELINEBREADCRUMB_H_
