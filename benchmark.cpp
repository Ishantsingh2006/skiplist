#include <benchmark/benchmark.h>
#include "skiplist.hpp"
#include <random>
#include <thread>
#include <vector>
#include <memory>
#include <mutex>
#include <cmath>

// --- Pre-generated Random Inputs to Eliminate RNG Overhead from timing loops ---

struct MixedOp
{
    int op_type;
    int key;
};

static std::vector<int> generate_random_keys(size_t count)
{
    std::vector<int> keys(count);
    std::mt19937 rng(1337);
    for (size_t i = 0; i < count; ++i)
    {
        keys[i] = rng();
    }
    return keys;
}

static const std::vector<int> &get_global_random_keys()
{
    static std::vector<int> keys = generate_random_keys(1000000);
    return keys;
}

static const std::vector<MixedOp> &get_global_mixed_ops()
{
    static std::vector<MixedOp> ops;
    static std::once_flag flag;
    std::call_once(flag, []()
                   {
        std::mt19937 rng(1337);
        ops.resize(1000000);
        for (size_t i = 0; i < ops.size(); ++i) {
            ops[i].op_type = rng() % 100;
            ops[i].key = rng() % 20000;
        } });
    return ops;
}

// --- Static SkipList instances to prevent thread lifetime races ---

static custom::ConcurrentSkipList<int, int> &get_insert_seq_list()
{
    static custom::ConcurrentSkipList<int, int> sl(16, 0.5f);
    return sl;
}

static custom::ConcurrentSkipList<int, int> &get_insert_rand_list()
{
    static custom::ConcurrentSkipList<int, int> sl(16, 0.5f);
    return sl;
}

static custom::ConcurrentSkipList<int, int> &get_contain_seq_list()
{
    static custom::ConcurrentSkipList<int, int> sl(16, 0.5f);
    static std::once_flag flag;
    std::call_once(flag, []()
                   {
        for (int i = 0; i < 50000; ++i) sl.insert({i, i}); });
    return sl;
}

static custom::ConcurrentSkipList<int, int> &get_contain_rand_list()
{
    static custom::ConcurrentSkipList<int, int> sl(16, 0.5f);
    static std::once_flag flag;
    std::call_once(flag, []()
                   {
        for (int i = 0; i < 50000; ++i) sl.insert({i, i}); });
    return sl;
}

static custom::ConcurrentSkipList<int, int> &get_remove_seq_list()
{
    static custom::ConcurrentSkipList<int, int> sl(16, 0.5f);
    static std::once_flag flag;
    std::call_once(flag, []()
                   {
        for (int i = 0; i < 50000; ++i) sl.insert({i, i}); });
    return sl;
}

static custom::ConcurrentSkipList<int, int> &get_remove_rand_list()
{
    static custom::ConcurrentSkipList<int, int> sl(16, 0.5f);
    static std::once_flag flag;
    std::call_once(flag, []()
                   {
        for (int i = 0; i < 50000; ++i) sl.insert({i, i}); });
    return sl;
}

static custom::ConcurrentSkipList<int, int> &get_mixed_equal_list()
{
    static custom::ConcurrentSkipList<int, int> sl(16, 0.5f);
    static std::once_flag flag;
    std::call_once(flag, []()
                   {
        for (int i = 0; i < 10000; ++i) sl.insert({i, i}); });
    return sl;
}

static custom::ConcurrentSkipList<int, int> &get_mixed_read_list()
{
    static custom::ConcurrentSkipList<int, int> sl(16, 0.5f);
    static std::once_flag flag;
    std::call_once(flag, []()
                   {
        for (int i = 0; i < 10000; ++i) sl.insert({i, i}); });
    return sl;
}

static custom::ConcurrentSkipList<int, int> &get_mixed_write_list()
{
    static custom::ConcurrentSkipList<int, int> sl(16, 0.5f);
    static std::once_flag flag;
    std::call_once(flag, []()
                   {
        for (int i = 0; i < 10000; ++i) sl.insert({i, i}); });
    return sl;
}

// --- 1. Insertion Benchmarks ---

static void BM_Insert_Sequential(benchmark::State &state)
{
    auto &sl = get_insert_seq_list();
    int base = state.thread_index() * 1000000;
    int offset = 0;
    for (auto _ : state)
    {
        sl.insert({base + offset, offset});
        offset++;
    }
}
BENCHMARK(BM_Insert_Sequential)->ThreadRange(1, 8);

static void BM_Insert_Random(benchmark::State &state)
{
    auto &sl = get_insert_rand_list();
    const auto &keys = get_global_random_keys();
    size_t base_idx = (state.thread_index() * 100000) % keys.size();
    size_t idx = 0;
    for (auto _ : state)
    {
        int key = keys[(base_idx + idx) % keys.size()];
        sl.insert({key, key});
        idx++;
    }
}
BENCHMARK(BM_Insert_Random)->ThreadRange(1, 8);

// --- 2. Contain Benchmarks ---

static void BM_Contain_Sequential(benchmark::State &state)
{
    auto &sl = get_contain_seq_list();
    int base = (state.thread_index() * 5000) % 50000;
    int offset = 0;
    for (auto _ : state)
    {
        int key = (base + offset) % 50000;
        benchmark::DoNotOptimize(sl.contains(key));
        offset++;
    }
}
BENCHMARK(BM_Contain_Sequential)->ThreadRange(1, 8);

static void BM_Contain_Random(benchmark::State &state)
{
    auto &sl = get_contain_rand_list();
    const auto &keys = get_global_random_keys();
    size_t base_idx = (state.thread_index() * 100000) % keys.size();
    size_t idx = 0;
    for (auto _ : state)
    {
        int key = std::abs(keys[(base_idx + idx) % keys.size()]) % 50000;
        benchmark::DoNotOptimize(sl.contains(key));
        idx++;
    }
}
BENCHMARK(BM_Contain_Random)->ThreadRange(1, 8);

// --- 3. Remove Benchmarks ---

static void BM_Remove_Sequential(benchmark::State &state)
{
    auto &sl = get_remove_seq_list();
    int base = state.thread_index() * 5000;
    int offset = 0;
    for (auto _ : state)
    {
        int key = (base + offset) % 50000;
        sl.erase(key);
        offset++;
    }
}
BENCHMARK(BM_Remove_Sequential)->ThreadRange(1, 8);

static void BM_Remove_Random(benchmark::State &state)
{
    auto &sl = get_remove_rand_list();
    const auto &keys = get_global_random_keys();
    size_t base_idx = (state.thread_index() * 100000) % keys.size();
    size_t idx = 0;
    for (auto _ : state)
    {
        int key = std::abs(keys[(base_idx + idx) % keys.size()]) % 50000;
        sl.erase(key);
        idx++;
    }
}
BENCHMARK(BM_Remove_Random)->ThreadRange(1, 8);

// --- 4. Operation Mixtures ---

static void BM_Mixed_Equal(benchmark::State &state)
{
    auto &sl = get_mixed_equal_list();
    const auto &ops = get_global_mixed_ops();
    size_t base_idx = (state.thread_index() * 100000) % ops.size();
    size_t idx = 0;
    for (auto _ : state)
    {
        const auto &step = ops[(base_idx + idx) % ops.size()];
        if (step.op_type < 30)
        {
            sl.insert({step.key, step.key});
        }
        else if (step.op_type < 70)
        {
            benchmark::DoNotOptimize(sl.contains(step.key));
        }
        else
        {
            sl.erase(step.key);
        }
        idx++;
    }
}
BENCHMARK(BM_Mixed_Equal)->ThreadRange(1, 8);

static void BM_Mixed_ReadHeavy(benchmark::State &state)
{
    auto &sl = get_mixed_read_list();
    const auto &ops = get_global_mixed_ops();
    size_t base_idx = (state.thread_index() * 100000) % ops.size();
    size_t idx = 0;
    for (auto _ : state)
    {
        const auto &step = ops[(base_idx + idx) % ops.size()];
        if (step.op_type < 20)
        {
            sl.insert({step.key, step.key});
        }
        else if (step.op_type < 90)
        {
            benchmark::DoNotOptimize(sl.contains(step.key));
        }
        else
        {
            sl.erase(step.key);
        }
        idx++;
    }
}
BENCHMARK(BM_Mixed_ReadHeavy)->ThreadRange(1, 8);

static void BM_Mixed_WriteHeavy(benchmark::State &state)
{
    auto &sl = get_mixed_write_list();
    const auto &ops = get_global_mixed_ops();
    size_t base_idx = (state.thread_index() * 100000) % ops.size();
    size_t idx = 0;
    for (auto _ : state)
    {
        const auto &step = ops[(base_idx + idx) % ops.size()];
        if (step.op_type < 80)
        {
            sl.insert({step.key, step.key});
        }
        else
        {
            benchmark::DoNotOptimize(sl.contains(step.key));
        }
        idx++;
    }
}
BENCHMARK(BM_Mixed_WriteHeavy)->ThreadRange(1, 8);

BENCHMARK_MAIN();
