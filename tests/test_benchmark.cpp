#include <gtest/gtest.h>
#include "../src/shared/types.hpp"
#include "../src/agent/controller.hpp"
#include <chrono>
#include <iostream>

using namespace vigilantune;
using namespace nanoloop;

TEST(BenchmarkTest, TagDatabaseLatency) {
    TagDatabase db;
    const int iterations = 1000000; // 1 Million Ops

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        db.set(TagID::CPU_Utilization, 50.0);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    double ns_per_op = static_cast<double>(duration_ns) / iterations;

    std::cout << "[Benchmark] TagDatabase::set -> " << ns_per_op << " ns/op" << std::endl;
    
    // A lock-free atomic set should easily be sub-10ns on modern CPUs.
    // We assert it's well below the overhead of a mutex (typically 20-50ns+ uncontended).
    EXPECT_LT(ns_per_op, 30.0);
}

TEST(BenchmarkTest, ControlLoopZeroAllocation) {
    Controller controller;
    TagDatabase db;
    
    db.set(TagID::CPU_Utilization, 85.0);
    db.set(TagID::Thread_Queue_Length, 10.0);
    
    // Warmup
    controller.evaluate(db);
    
    const int iterations = 100000; // 100k Ops
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        // Evaluate the dirty state continuously
        auto result = controller.evaluate(db);
        // Prevent aggressive compiler optimization
        EXPECT_GE(result.stress_score, 0.0); 
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    double us_per_op = static_cast<double>(duration_us) / iterations;

    std::cout << "[Benchmark] Controller::evaluate (Dirty Check) -> " << us_per_op << " us/op" << std::endl;
    
    // The zero-allocation path should be blazing fast, sub-microsecond
    EXPECT_LT(us_per_op, 1.0);
}
