# by-key

[![CI](https://github.com/tremolo-delay/by-key/actions/workflows/ci.yml/badge.svg)](https://github.com/tremolo-delay/by-key/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

Header-only C++20 utilities for grouping, counting, and indexing ranges by a derived key. The library wraps a handful of small algorithms so you can turn ranges of values into associative containers with deterministic ordering helpers when you need to inspect the results.

## Highlights
- Works with any `std::ranges::input_range`
- Lets you choose how keys and values are projected, including overwriting or keep-first semantics
- Adds one-liners for grouping, reduction, partitioning, and per-key extrema
- Provides deterministic helpers (`to_sorted_pairs`, `top_k`, `top_k_by_value`, `bottom_k_by_value`) for reporting or testing
- Pipeline adaptors enable `range | bykey::adaptors::...` style composition
- Ships as a single header (`include/by-key/by_key.hpp`)

## Getting Started

### Requirements
- CMake 3.20+
- A C++20-capable compiler (GCC 11+, Clang 13+, MSVC 19.3+)

### Adding to a Project
Vendor the repository (for example using `add_subdirectory` or FetchContent) and link the interface target:

```cmake
add_subdirectory(by-key)
target_link_libraries(my_app PRIVATE bykey)
```

Include the header from your sources:

```cpp
#include <by-key/by_key.hpp>
```

## Usage Example

```cpp
#include <algorithm>
#include <iostream>
#include <ranges>
#include <string>
#include <vector>
#include <by-key/by_key.hpp>

int main() {
    std::vector<std::string> words{"eat","tea","tan","ate","nat","bat"};

    auto groups = bykey::group_by(
        words,
        [](std::string s){ std::ranges::sort(s); return s; });

    auto ordered = bykey::to_sorted_pairs(
        groups,
        [](auto const& a, auto const& b){ return a.first < b.first; });

    for (auto const& [signature, bucket] : ordered) {
        std::cout << signature << ": " << bucket.size() << " variants\n";
    }
}
```

## API Overview

### Transform reduce with custom traits

Traits let you plug in reusable identities, combination logic, and optional finalisation steps:

```cpp
struct average_traits {
    struct state { double sum = 0.0; int count = 0; };
    auto identity() const { return state{}; }
    void combine(state& s, int value) const { s.sum += value; ++s.count; }
    double finalize(state const& s) const { return s.count ? s.sum / s.count : 0.0; }
};

struct row { std::string team; int score; };
std::vector<row> scores{{"red",3},{"blue",4},{"red",5}};

auto averages = bykey::transform_reduce_by(
    scores,
    [](const row& r){ return r.team; },
    [](const row& r){ return r.score; },
    average_traits{});
// averages["red"] == 4.0, averages["blue"] == 4.0
```

### Pipeline adaptors

All algorithms are available as lightweight adaptors, making ranged pipelines ergonomic:

```cpp
std::vector<int> numbers{1,1,2,3,5,8,13};

auto remainder_counts = numbers | bykey::adaptors::count([](int x){ return x % 3; });
auto parity_groups    = numbers | bykey::adaptors::group([](int x){ return x % 2; });
auto parity_sums      = numbers | bykey::adaptors::accumulate([](int x){ return x % 2; }, [](int x){ return x; });
auto partitions       = numbers | bykey::adaptors::partition([](int x){ return x < 5; });
```

## Example Problems

- 49. Group Anagrams (`examples/lc_0049_group_anagrams.cpp`): group words by sorted-letter signatures; output order is immaterial and buckets reuse `group_by`.
- 347. Top K Frequent Elements (`examples/lc_0347_top_k_frequent.cpp`): count integers and slice the most frequent keys with `top_k_by_value`.
- 697. Degree of an Array (`examples/lc_0697_degree_of_array.cpp`): track per-value first/last indices via `minmax_by`, then search for the shortest subarray that matches the global degree.
- 350. Intersection of Two Arrays II (`examples/lc_0350_intersection_ii.cpp`): build frequency maps with `count_by` and decrement while scanning the second list to emit the multiset intersection.
- 242. Valid Anagram (`examples/lc_0242_valid_anagram.cpp`): compare two `count_by` maps to decide whether strings are anagrams.
- 1331. Rank Transform of an Array (`examples/lc_1331_rank_transform.cpp`): project unique sorted values into 1-based ranks with `index_by`, then map the input through the resulting lookup.

These workflows double as unit tests (`tests/test_by_key.cpp`) so CI validates each recipe alongside the standalone example binaries.

More idea starters: `count_by` unlocks answers for 451 Sort Characters by Frequency, 169 Majority Element, and 1207 Unique Number of Occurrences; `index_by` solves 599 Minimum Index Sum of Two Lists; `accumulate_by` keeps score totals for 2225 Find Players With Zero or One Losses.

Key functions at a glance:

- `count_by(range, key_projection, expected_unique = 0)`: returns an `unordered_map` of key frequencies.
- `index_by(range, key_projection, value_projection, overwrite = true)` / `index_by_into(...)`: build maps of projected keys and values with overwrite or keep-first behaviour.
- `group_reduce_by(range, key_projection, value_projection, initial_value, reducer, expected_unique = 0)`: general-purpose grouping with a custom accumulator.
- `group_by(range, key_projection, value_projection = {}, expected_unique = 0)` / `group_by_into(...)`: create vectors of values per key without writing the reducer boilerplate.
- `transform_reduce_by(range, key_projection, value_projection, traits_or_init, ...)` and `accumulate_by(...)`: compute per-key scalars (sums, averages, custom reductions via traits or binary ops).
- `extrema_by(range, key_projection, value_projection, order_projection = value_projection, comparator = std::ranges::less)` / `minmax_by(...)`: capture the elements (or values) that produce per-key minima and maxima.
- `partition_by(range, predicate, value_projection = {})`: split a range into `partition_result` holding values for the false/true branches.
- `to_sorted_pairs(map, comparator)`, `top_k(map, k, comparator)`, `top_k_by_key(map, k)`, `top_k_by_value(map, k)`, `bottom_k_by_value(map, k)`: deterministic ordering helpers for reporting and slicing.
- `bykey::adaptors::{count, group, accumulate, transform_reduce, extrema, partition}`: pipeline-friendly wrappers so you can write `range | bykey::adaptors::count(...)`.

All algorithms participate in type-deduction; use lambdas or callable objects to shape keys and values as needed. Because each function returns an owning container, you can freely adapt results or hand them to further algorithms.

## Building and Testing

Configure the project and build the tests:

```bash
cmake -S . -B build
cmake --build build
cd build && ctest
```

Tests use GoogleTest. A preconfigured FetchContent block pulls a known-good release by default; set `-DUSE_SYSTEM_GTEST=ON` to rely on a system installation instead.

## Contributing

Issues and pull requests are welcome. Please run the test suite (`ctest`) before submitting changes.

## License

Distributed under the MIT License. See `LICENSE` for details.
