// Fix: cooperative_groups → HIP direct thread indexing
#include "cuda_ops.hip.h"
#include <hip/hip_runtime.h>
#define cg_block thread_group()
#define cg_tile thread_group()
#define cg_warp thread_group()
// cooperative_groups 简化: 直接用 threadIdx/blockIdx
namespace cg { inline auto this_thread_block() { return 0; } inline auto tiled_partition(int) { return 0; } }
