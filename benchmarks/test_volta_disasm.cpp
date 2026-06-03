#include "device_backend.h"
#include "backends/volta_backend.h"
#include <cstdio>
int main() {
    sass::VoltaBackend vb;
    uint64_t tests[] = {
        0x00000a0000017a02ULL, 0x0000000000047919ULL, 0x0000000000007918ULL,
        0x000000000000794dULL, 0xfffffff000007947ULL, 0x0000000000007b1dULL,
        0x00005a0004027625ULL, 0x0000000002027381ULL, 0x0000000904007386ULL,
        0x0000000200007306ULL, 0x0000003f0000780cULL, 0x0000000204077819ULL,
    };
    const char* expect[] = {"mov","s2r","nop","exit","bra","bar.sync",
        "imad.wide","ldg","stg","i2f","isetp","shf"};
    for (int i = 0; i < 12; i++)
        printf("%-12s -> %s  %s\n", expect[i], vb.disassemble(tests[i]).c_str(),
               vb.disassemble(tests[i]) == expect[i] ? "OK" : "FAIL");
}
