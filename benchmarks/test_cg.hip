#include <hip/hip_runtime.h>
#include <hip/hip_cooperative_groups.h>
namespace cg = cooperative_groups;
__global__ void test_cg(int* d) {
    auto tb = cg::this_thread_block();
    if (tb.thread_rank() == 0) d[0] = tb.size();
}
int main() {
    int *d; hipMalloc(&d, 4);
    test_cg<<<1,256>>>(d);
    int h; hipMemcpy(&h, d, 4, hipMemcpyDeviceToHost);
    printf("cooperative_groups: block_size=%d %s\n", h, h==256?"OK":"FAIL");
    hipFree(d); return 0;
}
