# Concurrent Skip List

A high-performance concurrent Skip List implementation in C++. It provides a map-like associative container that supports multi-reader concurrency with safe concurrent reads and exclusive writes.

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
    custom::ConcurrentSkipList<int, std::string> sl;
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
|  **1 Thread** | 1,196 ops/ms            | 668 ops/ms           |
|  **2 Threads**| 872 ops/ms              | 461 ops/ms           |
|  **4 Threads**| 745 ops/ms              | 438 ops/ms           |
|  **8 Threads**| 627 ops/ms              | 432 ops/ms           |

<img width="700" height="450" alt="insert_performance" src="https://github.com/user-attachments/assets/4b448749-a5da-42f3-bb83-7f3d5bcd8f0d" />


### 2. Contain (Lookup) Performance
Measures lookup throughput (in operations per millisecond). Since lookups run under a shared lock, multiple reader threads can search concurrently without blocking each other.

| Thread Count | Sequential Lookup | Random Lookup |
|:------------:|:-----------------:|:-------------:|
|  **1 Thread** | 3,184 ops/ms      | 2,136 ops/ms  |
|  **2 Threads**| 5,882 ops/ms      | 3,724 ops/ms  |
|  **4 Threads**| 10,752 ops/ms     | 7,393 ops/ms  |
|  **8 Threads**| 18,779 ops/ms     | 12,084 ops/ms |
<img width="700" height="450" alt="contain_performance" src="https://github.com/user-attachments/assets/fb97342d-18e2-40c6-9426-61350ddd363d" />


### 3. Remove (Deletion) Performance
Measures deletion throughput (in operations per millisecond) under concurrent deletion workloads.

| Thread Count | Sequential Remove | Random Remove |
|:------------:|:-----------------:|:-------------:|
|  **1 Thread** | 4,405 ops/ms      | 4,273 ops/ms  |
|  **2 Threads**| 1,904 ops/ms      | 2,148 ops/ms  |
|  **4 Threads**| 1,828 ops/ms      | 1,700 ops/ms  |
|  **8 Threads**| 1,591 ops/ms      | 1,612 ops/ms  |

<img width="700" height="450" alt="remove_performance" src="https://github.com/user-attachments/assets/73dd1c12-5fb7-4c6d-8d44-461d904ecce5" />


### 4. Mixed Workload Distributions
Measures throughput under mixed workloads:
* **Equal Distribution**: 30% insert, 40% contain, 30% remove.
* **Read-Heavy**: 20% insert, 70% contain, 10% remove.
* **Write-Heavy**: 80% insert, 20% contain, 0% remove.

| Thread Count | Equal (30/40/30) | Read-Heavy (20/70/10) | Write-Heavy (80/20/0) |
|:------------:|:----------------:|:---------------------:|:---------------------:|
|  **1 Thread** | 1,506 ops/ms     | 1,941 ops/ms          | 1,709 ops/ms          |
|  **2 Threads**| 721 ops/ms       | 737 ops/ms            | 869 ops/ms            |
|  **4 Threads**| 697 ops/ms       | 678 ops/ms            | 866 ops/ms            |
|  **8 Threads**| 639 ops/ms       | 644 ops/ms            | 798 ops/ms            |

<img width="700" height="450" alt="mixed_performance" src="https://github.com/user-attachments/assets/5918f29b-9170-4ab8-ac29-7697a56d79f0" />


### Key Observations & Performance Analysis

* **Sequential vs. Random Write Behavior**:
  * **Theory vs. Reality**: In theory, multi-threaded random writes **should be faster** than sequential writes because they distribute updates across the Skip List, reducing node-level contention. However, in our real benchmark results, random has **lower throughput** (e.g., 668 ops/ms for random vs. 1196 ops/ms for sequential at 1 thread).
  * **Why Random is Not Faster**:
    1. **Coarse-Grained Locking**: Our implementation uses a coarse-grained read-write lock (`std::shared_mutex` with `std::unique_lock` for writes). This serializes all write operations globally, meaning threads cannot perform parallel modifications anyway. This eliminates any potential scaling advantage that a random distribution would offer.
    2. **Cache Misses**: Because writes are serialized, the performance is dominated by memory hardware performance. Sequential operations access adjacent memory chunks, preserving high cache locality, whereas random operations jump across arbitrary heap locations, causing constant L1/L2 cache misses.
* **Why Read Throughput Increases vs. Why Write Throughput Decreases**:
  * **Read-Only scaling (Increases)**: Since contain (lookup) operations acquire a shared lock (`std::shared_lock`), multiple threads read the Skip List simultaneously on different CPU cores. As the thread count increases, the total aggregate throughput of the system **increases (scales up)** because the CPU cores perform reads in parallel.
  * **Write-Only scaling (Decreases)**: Write operations require an exclusive lock (`std::unique_lock`). Only one thread can write at any moment while all other writer threads are blocked. Because writes are serialized, increasing the number of threads cannot increase write performance. Instead, throughput **decreases** due to the CPU cycles wasted on lock acquisition delays, operating system thread scheduling, and context-switching overhead.
* **Mixed Workload Behavior**:
  * The performance of mixed workloads is directly dictated by the ratio of write operations. The *Read-Heavy* workload (20% insert, 70% contain, 10% remove) runs significantly faster and scales better than the *Write-Heavy* workload (80% insert, 20% contain) due to the higher utilization of concurrent shared locks.

---

### Member Functions & Time Complexity

| Function Signature | Description | Time Complexity (Average) |
|:---|:---|:---|
| `ConcurrentSkipList(size_t max_level = 32, float p = 0.5f)` | Constructs an empty Skip List with a maximum index height and level generation probability. | $O(1)$ |
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

---

## Requirements

This library requires a compiler that fully supports the **C++23** standard:
* **Compiler**: GCC 14+ (or Clang 18+)
* **Flag**: `-std=c++23` (or `-std=c++2b`)
