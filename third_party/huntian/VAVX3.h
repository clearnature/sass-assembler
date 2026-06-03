//===-- llvm/Support/VAVX3/VAVX3.h - Virtual AVX3 ISA Support --*- C++ -*-===//
//
// HunTian Virtual AVX3 Instruction Set Architecture
// 4320D/8640D Manifold Geometric Computing Primitives
//
// Key Features:
// - 512-bit virtual vector operations (vavx3_512i)
// - Geometric transformations (rotation, toroidal inversion)
// - Spectral geometry (Laplacian, curvature flow)
// - Topological braiding and entanglement
// - Phi^4 scaling and Yamabe flow
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SUPPORT_VAVX3_VAVX3_H
#define LLVM_SUPPORT_VAVX3_VAVX3_H

// Core VAVX3 primitives - always include
#include "vavx3_primitives.h"

// Core VAVX3 operations
#include "vavx3_core.h"

// VAVX3 axioms and theorems
#include "vavx3_axioms.h"

// HunTian integration
#include "llvm/Support/QuantumClock.h"
#include "llvm/Support/XuangongTuning.h"

#endif // LLVM_SUPPORT_VAVX3_VAVX3_H
