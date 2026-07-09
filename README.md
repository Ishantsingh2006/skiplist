# skiplist

A high-performance concurrent skiplist implementation in C++. It provides a map-like associative container that supports multi-reader concurrency with safe concurrent reads and exclusive writes.

---

## Core Features

* **Multi-Reader Concurrency**: Supports parallel readers using `std::shared_lock` and exclusive writers using `std::unique_lock` via a `std::shared_mutex`.
* **STL-Compatible Interface**: Implements standard associative container methods (`insert()`, `erase()`, `find()`, `contains()`, `operator[]`) and bidirectional iterators supporting range-based loops.
* **Visual Debugging & Diagnostics**: Built-in support for inspecting internal structural states including node height querying (`get_node_height()`), level-by-level printing (`print_level()`), level distribution rendering (`print_distribution()`), and automated integrity validation (`lacksIntegrity()`).

---

## Getting Started

To use the Skip List, include the header file directly in your C++ code:

```cpp
#include "skiplist.hpp"

int main() {
    skip::skiplist<int, std::string> sl;
    sl.insert({1, "one"});
    if (sl.contains(1)) {
        std::cout << sl.at(1) << std::endl;
    }
}
```

Compile with C++23 support:
```bash
g++ -std=c++23 main.cpp -o skiplist_demo
./skiplist_demo
```

---

## Running Tests

To run the comprehensive functional, concurrent, and memory-leak tests:

```bash
g++ -std=c++23 test.cpp -o skiplist_test
./skiplist_test
```

---

## Running Benchmarks

To compile and run the Google Benchmark performance tests:

```bash
# Compile benchmark binary
g++ -std=c++23 benchmark.cpp -o skiplist_benchmark -lbenchmark -lpthread

# Run and output results to JSON
./skiplist_benchmark --benchmark_out=results.json --benchmark_out_format=json

# Generate performance graphs
python3 plot.py
```

---

## Benchmark Results

### 1. Insertion Performance
Measures throughput (in operations per millisecond) under sequential vs. random key insertion patterns as the number of parallel threads increases.

| Thread Count | Sequential Key Insertion | Random Key Insertion |
|:------------:|:------------------------:|:--------------------:|
|  **1 Thread** | 1,197 ops/ms            | 623 ops/ms           |
|  **2 Threads**| 893 ops/ms              | 427 ops/ms           |
|  **4 Threads**| 697 ops/ms              | 438 ops/ms           |
|  **8 Threads**| 658 ops/ms              | 377 ops/ms           |

<img width="700" height="450" alt="insert_performance" src="https://github.com/user-attachments/assets/38f74842-882c-4594-9822-eee42eb529e2" />



### 2. Contain (Lookup) Performance
Measures lookup throughput (in operations per millisecond). Since lookups run under a shared lock, multiple reader threads can search concurrently without blocking each other.

| Thread Count | Sequential Lookup | Random Lookup |
|:------------:|:-----------------:|:-------------:|
|  **1 Thread** | 2,989 ops/ms      | 2,079 ops/ms  |
|  **2 Threads**| 5,594 ops/ms      | 3,871 ops/ms  |
|  **4 Threads**| 9,923 ops/ms      | 7,099 ops/ms  |
|  **8 Threads**| 16,197 ops/ms     | 11,516 ops/ms |
<img width="700" height="450" alt="contain_performance" src="https://github.com/user-attachments/assets/9c51d21a-29ab-4230-8052-28ce97008453" />



### 3. Remove (Deletion) Performance
Measures deletion throughput (in operations per millisecond) under concurrent deletion workloads.

| Thread Count | Sequential Remove | Random Remove |
|:------------:|:-----------------:|:-------------:|
|  **1 Thread** | 4,132 ops/ms      | 4,045 ops/ms  |
|  **2 Threads**| 2,068 ops/ms      | 1,954 ops/ms  |
|  **4 Threads**| 1,754 ops/ms      | 1,903 ops/ms  |
|  **8 Threads**| 1,728 ops/ms      | 1,760 ops/ms  |

<img width="700" height="450" alt="remove_performance" src="https://github.com/user-attachments/assets/b70f5c15-151a-45f6-891f-4f5964d0ca2d" />



### 4. Mixed Workload Distributions
Measures throughput under mixed workloads:
* **Equal Distribution**: 30% insert, 40% contain, 30% remove.
* **Read-Heavy**: 20% insert, 70% contain, 10% remove.
* **Write-Heavy**: 80% insert, 20% contain, 0% remove.

| Thread Count | Equal (30/40/30) | Read-Heavy (20/70/10) | Write-Heavy (80/20/0) |
|:------------:|:----------------:|:---------------------:|:---------------------:|
|  **1 Thread** | 1,631 ops/ms     | 1,836 ops/ms          | 1,587 ops/ms          |
|  **2 Threads**| 707 ops/ms       | 695 ops/ms            | 797 ops/ms            |
|  **4 Threads**| 662 ops/ms       | 649 ops/ms            | 696 ops/ms            |
|  **8 Threads**| 631 ops/ms       | 625 ops/ms            | 677 ops/ms            |

<img width="700" height="450" alt="mixed_performance" src="https://github.com/user-attachments/assets/8580f480-6e8e-4ecd-aec2-4e9b02ef03ac" />


### Key Observations & Performance Analysis

* **Sequential vs. Random Write Behavior**:
  * **Theory vs. Reality**: In theory, multi-threaded random writes **should be faster** than sequential writes because they distribute updates across the Skip List, reducing node-level contention. However, in our real benchmark results, random has **lower throughput** (e.g., 623 ops/ms for random vs. 1,197 ops/ms for sequential at 1 thread).
  * **Why Random is Not Faster**:
    1. **Coarse-Grained Locking**: Our implementation uses a coarse-grained read-write lock (`std::shared_mutex` with `std::unique_lock` for writes). This serializes all write operations globally, meaning threads cannot perform parallel modifications anyway. This eliminates any potential scaling advantage that a random distribution would offer.
    2. **Cache Misses**: Because writes are serialized, the performance is dominated by memory hardware performance. Sequential operations access adjacent memory chunks, preserving high cache locality, whereas random operations jump across arbitrary heap locations, causing constant L1/L2 cache misses.
* **Why Read Throughput Increases vs. Why Write Throughput Decreases**:
  * **Read-Only scaling (Increases)**: Since contain (lookup) operations acquire a shared lock (`std::shared_lock`), multiple threads read the Skip List simultaneously on different CPU cores. As the thread count increases, the total aggregate throughput of the system **increases (scales up)** because the CPU cores perform reads in parallel.
  * **Write-Only scaling (Decreases)**: Write operations require an exclusive lock (`std::unique_lock`). Only one thread can write at any moment while all other writer threads are blocked. Because writes are serialized, increasing the number of threads cannot increase write performance. Instead, throughput **decreases** due to the CPU cycles wasted on lock acquisition delays, operating system thread scheduling, and context-switching overhead.
* **Mixed Workload Behavior**:
    * For Thread 1: Read-Heavy is faster because there is no thread contention, allowing its 70% simple lookup operations to execute without lock serialization or allocator overhead.
    * For Threads >= 2: Write-Heavy becomes faster due to key saturation (0% removals) converting inserts into early-return duplicate checks. These duplicate checks avoid CPU-heavy heap allocations ( new ) and pointer mutations that would invalidate CPU cache lines. Conversely, Read-Heavy's 10% removal rate prevents saturation, forcing threads to continuously  allocate and deallocate memory. This triggers slow global memory allocator locks (in  malloc / free ) and cache misses across threads, bottlenecking multi-threaded performance.
---

### Member Functions & Time Complexity

| Function Signature | Description | Time Complexity (Average) |
|:---|:---|:---|
| `skiplist(size_t max_level = 32, float p = 0.5f)` | Constructs an empty Skip List with a maximum index height and level generation probability. | $O(1)$ |
| `std::pair<iterator, bool> insert(const value_type& value)` | Inserts a key-value pair. Returns an iterator to the node and `true` if inserted, `false` if key already exists. | $O(\log n)$ |
| `std::pair<iterator, bool> insert(value_type&& value)` | Moves and inserts a key-value pair. | $O(\log n)$ |
| `std::pair<iterator, bool> insert_or_assign(const Key& key, M&& obj)` | Inserts key-value pair or assigns value to key if it already exists. | $O(\log n)$ |
| `iterator erase(const_iterator pos)` | Removes the element at the iterator position and returns the next iterator. | $O(\log n)$ |
| `size_type erase(const Key& key)` | Removes the element with the specified key. Returns 1 if removed, 0 if not found. | $O(\log n)$ |
| `void clear() noexcept` | Removes all elements from the Skip List. | $O(n)$ |
| `T& at(const Key& key)` | Accesses the element value at the specified key. Throws `std::out_of_range` if key not found. | $O(\log n)$ |
| `const T& at(const Key& key) const` | Accesses the element value at the specified key (read-only const version). | $O(\log n)$ |
| `T& operator[](const Key& key)` | Accesses or inserts a default-constructed element at the specified key. | $O(\log n)$ |
| `iterator find(const Key& key)` | Finds the element with the specified key. Returns `end()` if not found. | $O(\log n)$ |
| `const_iterator find(const Key& key) const` | Finds the element with the specified key (const version). | $O(\log n)$ |
| `size_type count(const Key& key) const` | Returns the number of elements with the key (always 0 or 1). | $O(\log n)$ |
| `bool contains(const Key& key) const` | Checks if the key exists in the Skip List. | $O(\log n)$ |
| `bool empty() const noexcept` | Returns `true` if the container is empty, `false` otherwise. | $O(1)$ |
| `size_type size() const noexcept` | Returns the number of elements in the Skip List. | $O(1)$ |
| `size_type max_size() const noexcept` | Returns the theoretical maximum size of the container. | $O(1)$ |
| `iterator begin() noexcept` | Returns a bidirectional iterator to the first element. | $O(1)$ |
| `const_iterator begin() const noexcept` | Returns a read-only const iterator to the first element. | $O(1)$ |
| `const_iterator cbegin() const noexcept` | Returns a read-only const iterator to the first element. | $O(1)$ |
| `iterator end() noexcept` | Returns an iterator to the end sentinel. | $O(1)$ |
| `const_iterator end() const noexcept` | Returns a read-only const iterator to the end sentinel. | $O(1)$ |
| `const_iterator cend() const noexcept` | Returns a read-only const iterator to the end sentinel. | $O(1)$ |
| `void print_level(size_t level, std::ostream& os = std::cout) const` | Prints space-separated keys present at a specific level (for debugging). | $O(n)$ |
| `int get_node_height(const Key& key) const` | Returns the height of a specific node (returns `-1` if not found). | $O(\log n)$ |
| `void print_distribution(std::ostream& os = std::cout) const` | Prints a visual layout showing level heights of all nodes (for debugging). | $O(n)$ |
| `std::expected<void, IntegrityError> lacksIntegrity() const` | Validates structural integrity (e.g. forward/back link coherence). | $O(n)$ |

### Mathematical Details on Search Time Complexity
While the average search complexity is documented as $O(\log n)$, the exact average complexity is dependent on the skip list configuration parameters: **probability $p$** and **maximum level $L$** (specified in the constructor).

The precise average search complexity is:
$$O\left(\frac{1}{p} \log_{1/p} n + L\right)$$

* **Role of $p$**: Controls the node height promotion probability. A value of $p = 0.5$ (default) creates a balanced base-2 search path (equivalent to a binary tree). If $p$ is too small (approaching 0) or too large (approaching 1), search time degenerates to $O(n)$.
* **Role of $L$**: Defines the maximum indexing level. The search starts at level $L - 1$ and descends, contributing $O(L)$ steps of overhead. For optimal performance, $L$ should be set close to $\log_{1/p}(N_{\text{max}})$, where $N_{\text{max}}$ is the maximum expected number of elements.

---

## Requirements

This library requires a compiler that fully supports the **C++23** standard:
* **Compiler**: GCC 14+ (or Clang 18+)
* **Flag**: `-std=c++23` (or `-std=c++2b`)
