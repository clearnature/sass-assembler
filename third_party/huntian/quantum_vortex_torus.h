#ifndef QUANTUM_VORTEX_TORUS_H
#define QUANTUM_VORTEX_TORUS_H

#include <stdint.h>
#include <math.h>

#define TERNARY_GRID_MAX 150094635296999121ULL
#define VORTEX_STRENGTH 0.0036f 

static inline uint64_t get_vortex_spiral_addr(uint64_t step, uint64_t width) {
    uint64_t base_addr = step % TERNARY_GRID_MAX;
    uint64_t rotation = (step / width) % 4; 
    float s = sinf((float)step * VORTEX_STRENGTH);
    uint64_t perturbation = (uint64_t)(fabsf(s) * 8.0f);
    return (base_addr + rotation + perturbation) % TERNARY_GRID_MAX;
}

#define __sdo_vortex_hot __attribute__((hot))

#endif
