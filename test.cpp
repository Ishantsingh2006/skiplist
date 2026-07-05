#define SKIPLIST_TEST_MEM
#include "skiplist.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <cassert>
#include <chrono>
#include <atomic>
#include <random>

void test_basic_operations() {
    std::cout << "Testing basic operations..." << std::endl;
    custom::ConcurrentSkipList<int, std::string> sl(8, 0.5f);

    assert(sl.empty());
    assert(sl.size() == 0);

    // Test insert
    auto [it1, inserted1] = sl.insert({10, "Ten"});
    assert(inserted1);
    assert(it1->first == 10);
    assert(it1->second == "Ten");
    assert(sl.size() == 1);

    // Duplicate insert should fail
    auto [it2, inserted2] = sl.insert({10, "Ten-New"});
    assert(!inserted2);
    assert(sl.size() == 1);
    assert(sl.at(10) == "Ten"); // value remains unchanged

    // Test insert_or_assign
    auto [it3, inserted3] = sl.insert_or_assign(10, "Ten-Assigned");
    assert(!inserted3);
    assert(sl.at(10) == "Ten-Assigned");
    assert(sl.size() == 1);

    // Add more elements
    sl.insert({20, "Twenty"});
    sl.insert({5, "Five"});
    sl.insert({15, "Fifteen"});
    assert(sl.size() == 4);

    // Test contains and find
    assert(sl.contains(10));
    assert(sl.contains(20));
    assert(sl.contains(5));
    assert(sl.contains(15));
    assert(!sl.contains(100));

    auto find_it = sl.find(15);
    assert(find_it != sl.end());
    assert(find_it->first == 15);
    assert(find_it->second == "Fifteen");

    // Test operator[]
    assert(sl[30] == ""); // default constructed
    sl[30] = "Thirty";
    assert(sl.at(30) == "Thirty");
    assert(sl.size() == 5);

    // Test forward iterator traversal
    std::vector<int> keys;
    for (auto const& [key, value] : sl) {
        keys.push_back(key);
    }
    assert(keys == std::vector<int>({5, 10, 15, 20, 30}));

    // Test backward iterator traversal
    std::vector<int> rev_keys;
    auto bit = sl.end();
    if (bit != sl.begin()) {
        do {
            --bit;
            rev_keys.push_back(bit->first);
        } while (bit != sl.begin());
    }
    assert(rev_keys == std::vector<int>({30, 20, 15, 10, 5}));

    // Test erase
    assert(sl.erase(15) == 1);
    assert(!sl.contains(15));
    assert(sl.size() == 4);
    assert(sl.erase(15) == 0); // already erased

    // Test lack of integrity check
    auto integrity = sl.lacksIntegrity();
    assert(integrity.has_value());

    std::cout << "  Basic operations: PASSED" << std::endl;
}

void test_memory_leak() {
    std::cout << "Testing memory leak protection..." << std::endl;

    // Initially, there should be 0 active nodes
    assert(custom::NodeBase::active_nodes == 0);

    {
        custom::ConcurrentSkipList<int, int> sl(16, 0.5f);
        for (int i = 0; i < 1000; ++i) {
            sl.insert({i, i});
        }
        assert(sl.size() == 1000);
        // Make sure nodes are allocated
        assert(custom::NodeBase::active_nodes == 1000);

        // Erase some nodes
        for (int i = 0; i < 300; ++i) {
            sl.erase(i);
        }
        assert(sl.size() == 700);
        assert(custom::NodeBase::active_nodes == 700);

        // Clear the list
        sl.clear();
        assert(sl.size() == 0);
        assert(custom::NodeBase::active_nodes == 0);

        // Insert again
        for (int i = 0; i < 500; ++i) {
            sl.insert({i, i});
        }
        assert(custom::NodeBase::active_nodes == 500);
    } // SkipList goes out of scope here; destructor will be called

    // After destruction, active nodes must return to 0
    assert(custom::NodeBase::active_nodes == 0);

    std::cout << "  Memory leak protection: PASSED" << std::endl;
}

void test_concurrency() {
    std::cout << "Testing concurrent read and write operations..." << std::endl;

    assert(custom::NodeBase::active_nodes == 0);

    {
        custom::ConcurrentSkipList<int, int> sl(16, 0.5f);
        
        const int num_threads = 8;
        const int ops_per_thread = 1000;
        std::vector<std::thread> threads;

        std::atomic<bool> start_signal{false};

        // Spawn writer threads (inserting unique key ranges)
        for (int t = 0; t < num_threads / 2; ++t) {
            threads.emplace_back([&sl, t, ops_per_thread, &start_signal]() {
                while (!start_signal) {
                    std::this_thread::yield();
                }
                for (int i = 0; i < ops_per_thread; ++i) {
                    int key = t * ops_per_thread + i;
                    sl.insert({key, key * 2});
                }
            });
        }

        // Spawn reader threads (performing lookup and iterations)
        for (int t = num_threads / 2; t < num_threads; ++t) {
            threads.emplace_back([&sl, num_threads, ops_per_thread, &start_signal]() {
                while (!start_signal) {
                    std::this_thread::yield();
                }
                std::mt19937 rng(1337);
                for (int i = 0; i < ops_per_thread; ++i) {
                    int key = rng() % (num_threads / 2 * ops_per_thread);
                    sl.contains(key);
                    
                    // Concurrently iterate (should not crash under reader locks)
                    int count = 0;
                    for (auto it = sl.begin(); it != sl.end() && count < 10; ++it) {
                        (void)it->first;
                        count++;
                    }
                }
            });
        }

        // Release threads simultaneously
        start_signal = true;

        for (auto& th : threads) {
            if (th.joinable()) {
                th.join();
            }
        }

        // Verify size
        assert(sl.size() == (num_threads / 2 * ops_per_thread));
        assert(custom::NodeBase::active_nodes == static_cast<int64_t>(sl.size()));

        // Verify list integrity under concurrent changes
        auto integrity = sl.lacksIntegrity();
        assert(integrity.has_value());
    }

    // List is destroyed, memory must be fully reclaimed
    assert(custom::NodeBase::active_nodes == 0);

    std::cout << "  Concurrent read and write: PASSED" << std::endl;
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "          RUNNING SKIP LIST TEST SUITE            " << std::endl;
    std::cout << "==================================================" << std::endl;

    try {
        test_basic_operations();
        test_memory_leak();
        test_concurrency();
    } catch (const std::exception& ex) {
        std::cerr << "Test suite failed with exception: " << ex.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test suite failed with unknown exception." << std::endl;
        return 1;
    }

    std::cout << "==================================================" << std::endl;
    std::cout << "          ALL TESTS PASSED SUCCESSFULLY!          " << std::endl;
    std::cout << "==================================================" << std::endl;
    return 0;
}
