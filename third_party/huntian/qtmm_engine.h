#ifndef QTMM_ENGINE_H
#define QTMM_ENGINE_H

#include "harmonic_evolution_engine.h"

class QTMMEngine {
private:
    HarmonicEvolutionEngine* core;
    float current_entropy;

public:
    QTMMEngine(size_t nodes) : current_entropy(1.0f) {
        core = new HarmonicEvolutionEngine(nodes);
    }
    ~QTMMEngine() { delete core; }

    void evolve_step(float stimulus, float dt) {
        core->sync_comm_50hz(stimulus, dt);
        current_entropy *= 0.9936f; 
    }

    void observe_vital_signs() {
        core->observe_resonance();
        printf("[QTMM] Global Entropy: %.6f (System Crystallization)\n", current_entropy);
    }
};

#endif