#include "skiplist.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <cassert>
#include <chrono>

void run_basic_demo()
{
    std::cout << "==================================================\n";
    std::cout << "               RUNNING BASIC DEMO                 \n";
    std::cout << "==================================================\n";

    // Initialize with explicit max level (8) and probability p (0.5f)
    skip::skiplist<int, std::string> sl(8, 0.5f);

    sl.insert({10, "Ten"});
    sl.insert({20, "Twenty"});
    sl.insert({5, "Five"});
    sl.insert({15, "Fifteen"});
    sl.insert({30, "Thirty"});
    sl.insert({25, "Twenty-Five"});

    std::cout << "Printing nodes at each level:\n";
    for (size_t i = 0; i < sl.max_level(); ++i)
    {
        sl.print_level(i);
    }
    std::cout << "\n";

    std::cout << "Printing Skip List distribution:\n";
    sl.print_distribution();
    std::cout << "\n";

    std::cout << "Node heights (including key 99 which does not exist):\n";
    for (int key : {5, 10, 15, 20, 25, 30, 99})
    {
        std::cout << sl.get_node_height(key) << "\n";
    }
    std::cout << "\n";

    std::cout << "Lookup keys:\n";
    std::cout << "  Key 15: " << sl.at(15) << "\n";
    std::cout << "  Key 25: " << sl.at(25) << "\n";
    std::cout << "  Contains 10? " << (sl.contains(10) ? "Yes" : "No") << "\n";
    std::cout << "  Contains 99? " << (sl.contains(99) ? "Yes" : "No") << "\n\n";

    std::cout << "Forward iteration through skip list:\n  ";
    for (auto const &[key, value] : sl)
    {
        std::cout << "[" << key << ":" << value << "] -> ";
    }
    std::cout << "nullptr\n\n";

    std::cout << "Backward iteration (using -- operator) through skip list:\n  ";
    auto it = sl.end();
    if (it != sl.begin())
    {
        do
        {
            --it;
            std::cout << "[" << it->first << ":" << it->second << "] -> ";
        } while (it != sl.begin());
    }
    std::cout << "Sentinel\n\n";

    std::cout << "Erasing key 15 and 5...\n";
    sl.erase(15);
    sl.erase(5);

    std::cout << "After erasures, printing nodes at each level:\n";
    for (size_t i = 0; i < sl.max_level(); ++i)
    {
        sl.print_level(i);
    }
    std::cout << "\n";

    std::cout << "Backward iteration after erasures:\n  ";
    it = sl.end();
    if (it != sl.begin())
    {
        do
        {
            --it;
            std::cout << "[" << it->first << ":" << it->second << "] -> ";
        } while (it != sl.begin());
    }
    std::cout << "Sentinel\n\n";

    auto integrity = sl.lacksIntegrity();
    if (integrity)
    {
        std::cout << "Integrity Check: PASSED (OK)\n\n";
    }
    else
    {
        std::cout << "Integrity Check: FAILED with error code " << static_cast<int>(integrity.error()) << "\n\n";
    }
}

void run_concurrency_demo()
{
    std::cout << "==================================================\n";
    std::cout << "             RUNNING CONCURRENCY DEMO             \n";
    std::cout << "==================================================\n";

    // Initialize with explicit max level (16) and probability p (0.5f)
    skip::skiplist<int, int> sl(16, 0.5f);

    const int num_writers = 4;
    const int num_readers = 4;
    const int ops_per_thread = 500;

    std::vector<std::thread> threads;

    std::cout << "Spawning " << num_writers << " writer threads and "
              << num_readers << " reader threads executing "
              << ops_per_thread << " operations each...\n";

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < num_writers; ++t)
    {
        auto writer_task = [&sl, t, ops_per_thread]()
        {
            for (int i = 0; i < ops_per_thread; ++i)
            {
                int key = t * ops_per_thread + i;
                sl.insert({key, key * 10});

                if (i % 10 == 0)
                {
                    sl.erase(key - 5);
                }
            }
        };
        threads.emplace_back(writer_task);
    }

    for (int t = 0; t < num_readers; ++t)
    {
        auto reader_task = [&sl, ops_per_thread]()
        {
            for (int i = 0; i < ops_per_thread; ++i)
            {
                int key = rand() % (num_writers * ops_per_thread);
                sl.contains(key);
                try
                {
                    sl.at(key);
                }
                catch (const std::out_of_range &)
                {
                    // Ignore out of range exceptions
                }
            }
        };
        threads.emplace_back(reader_task);
    }

    for (auto &th : threads)
    {
        if (th.joinable())
        {
            th.join();
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    std::cout << "All threads finished in " << elapsed << " ms.\n";
    std::cout << "Final Skip List Size: " << sl.size() << "\n";

    auto integrity = sl.lacksIntegrity();
    if (integrity)
    {
        std::cout << "Integrity Check: PASSED (OK)\n";
    }
    else
    {
        std::cout << "Integrity Check: FAILED with error code " << static_cast<int>(integrity.error()) << "\n";
    }

    std::cout << "Printing distribution:\n";
    sl.print_distribution();
    std::cout << "==================================================\n";
}

int main()
{
    srand(static_cast<unsigned>(time(nullptr)));

    run_basic_demo();
    run_concurrency_demo();

    return 0;
}
