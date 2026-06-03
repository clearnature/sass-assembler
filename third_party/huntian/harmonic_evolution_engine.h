#ifndef HARMONIC_EVOLUTION_ENGINE_H
#define HARMONIC_EVOLUTION_ENGINE_H

#include "geometric_dynamics_vortex.h"
#include <vector>
#include <iostream>

#define BASE_50_FREQ 50.0f          // 切换为 50Hz 基频
#define COMM_INTERFACE_50HZ 50.0f    
#define RESONANCE_THRESHOLD 0.9536f  

struct HarmonicGene {
    float amplitude; 
    float phase;     
    float frequency; 
    HarmonicGene() : amplitude(1.0f), phase(0.0f), frequency(BASE_50_FREQ) {}
};

class HarmonicEvolutionEngine {
private:
    std::vector<HarmonicGene> wave_genes;
    float global_resonance_state;

public:
    HarmonicEvolutionEngine(size_t num_nodes) : wave_genes(num_nodes), global_resonance_state(0.0f) {}

    void sync_comm_50hz(float external_signal, float delta_time) {
        static float timer = 0.0f;
        timer += delta_time;
        // 50Hz 采样与基频同步
        if (timer >= (1.0f / COMM_INTERFACE_50HZ)) {
            this->apply_interference(external_signal);
            timer = 0.0f;
        }
    }

    void apply_interference(float error_signal) {
        #pragma GCC ivdep
        for (auto& gene : wave_genes) {
            float coupling = cosf(gene.phase);
            // 谐波微调：调整波幅和相位，而不是传统权重
            gene.amplitude += error_signal * coupling * 0.0036f;
            gene.phase += error_signal * (1.0f - coupling) * 0.01f;
        }
    }

    void observe_resonance() {
        float total_coherence = 0.0f;
        for (const auto& gene : wave_genes) {
            total_coherence += cosf(gene.frequency - BASE_50_FREQ);
        }
        this->global_resonance_state = total_coherence / wave_genes.size();
        printf("[Resonance Observer] 50Hz Coherence: %.6f | State: %s\n", 
               global_resonance_state,
               global_resonance_state > RESONANCE_THRESHOLD ? "LOCKED" : "EVOLVING");
    }
};

#endif