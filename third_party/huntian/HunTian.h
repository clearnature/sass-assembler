//===-- llvm/Support/HunTian/HunTian.h - HunTian System Integration -------===//
//
// HunTian Geometric Flow Optimization System
// Complete integration header for all HunTian components
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SUPPORT_HUNTIAN_HUNTIAN_H
#define LLVM_SUPPORT_HUNTIAN_HUNTIAN_H

// === Core Axioms ===
#include "code_life_axioms.h"
#include "THE_CONSTITUTION.h"

// === VAVX3 Virtual Instruction Set ===
#include "virtual_avx3_core.h"
#include "vavx3_fixed_math.h"

// === Geometric Engines ===
#include "geometric_dynamics_vortex.h"
#include "harmonic_evolution_engine.h"

// === Quantum Components ===
#include "quantum_state_baoyuan.h"
#include "quantum_vortex_torus.h"

// === Abstract ISA ===
#include "abstract_isa.h"

// === QTMM Engine ===
#include "qtmm_engine.h"

// === LLVM Integration ===
#include "llvm/Support/VAVX3/VAVX3.h"
#include "llvm/Support/QuantumClock.h"
#include "llvm/Support/XuangongTuning.h"

// HunTian System Version
#define HUNTIAN_VERSION_MAJOR 5
#define HUNTIAN_VERSION_MINOR 0
#define HUNTIAN_VERSION_PATCH 0

// Key Constants
#define HUNTIAN_PHI 1.6180339887
#define HUNTIAN_DIMENSIONS 8640
#define HUNTIAN_TARGET_FREQ 144.0
#define HUNTIAN_N14_NQR_FREQ 3170000.0

#endif // LLVM_SUPPORT_HUNTIAN_HUNTIAN_H
