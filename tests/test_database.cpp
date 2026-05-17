#include <gtest/gtest.h>
#include "../src/shared/types.hpp"
#include <thread>
#include <vector>
#include <atomic>

using namespace wspa;

TEST(TagDatabaseTest, ConcurrencyStress) {
    TagDatabase db;
    std::atomic<bool> start{false};
    const int num_threads = 20;
    const int iterations = 1000;
    
    std::vector<std::thread> threads;
    
    // Writer threads
    for (int i = 0; i < num_threads / 2; ++i) {
        threads.emplace_back([&db, &start, i, iterations]() {
            while (!start) std::this_thread::yield();
            for (int j = 0; j < iterations; ++j) {
                db.set("CPU_Utilization", (double)j);
                db.set("Foreground_App", std::string("App_") + std::to_string(i));
            }
        });
    }
    
    // Reader threads
    for (int i = 0; i < num_threads / 2; ++i) {
        threads.emplace_back([&db, &start, iterations]() {
            while (!start) std::this_thread::yield();
            for (int j = 0; j < iterations; ++j) {
                auto snapshot = db.get_all();
                (void)snapshot.size();
            }
        });
    }
    
    start = true;
    for (auto& t : threads) t.join();
    
    EXPECT_TRUE(db.contains("CPU_Utilization"));
    EXPECT_TRUE(db.contains("Foreground_App"));
}

TEST(TagDatabaseTest, SnapshotConsistency) {
    TagDatabase db;
    db.set("A", 1.0);
    db.set("B", 2);
    db.set("C", std::string("test"));
    
    auto snapshot = db.get_all();
    EXPECT_EQ(snapshot.size(), 3);
    EXPECT_EQ(std::get<double>(snapshot["A"].value), 1.0);
    EXPECT_EQ(std::get<int>(snapshot["B"].value), 2);
    EXPECT_EQ(std::get<std::string>(snapshot["C"].value), "test");
}
