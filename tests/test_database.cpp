#include <gtest/gtest.h>
#include "../src/shared/types.hpp"
#include <thread>
#include <vector>

using namespace wspa;

TEST(TagDatabaseTest, ConcurrencyStress) {
    TagDatabase db;
    const int num_threads = 10;
    const int iterations = 10000;
    
    auto writer = [&db](double val) {
        for (int i = 0; i < iterations; ++i) {
            db.set(TagID::CPU_Utilization, val);
        }
    };
    
    auto reader = [&db]() {
        double sum = 0;
        for (int i = 0; i < iterations; ++i) {
            sum += db.get(TagID::CPU_Utilization);
        }
        // Just ensuring no crash occurs
        EXPECT_GE(sum, 0); 
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        if (i % 2 == 0) threads.emplace_back(writer, (double)i);
        else threads.emplace_back(reader);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // Final state should be a valid written value
    double final_val = db.get(TagID::CPU_Utilization);
    EXPECT_GE(final_val, 0.0);
}

TEST(TagDatabaseTest, SnapshotConsistency) {
    TagDatabase db;
    db.set(TagID::CPU_Utilization, 42.5);
    db.set(TagID::Thread_Queue_Length, 10.0);
    
    TagSnapshot snapshot = db.get_snapshot();
    EXPECT_DOUBLE_EQ(snapshot.values[static_cast<size_t>(TagID::CPU_Utilization)], 42.5);
    EXPECT_DOUBLE_EQ(snapshot.values[static_cast<size_t>(TagID::Thread_Queue_Length)], 10.0);
}
