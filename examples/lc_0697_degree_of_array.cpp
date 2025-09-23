#include <algorithm>
#include <cassert>
#include <vector>

#include <by-key/by_key.hpp>

using namespace std;

int findShortestSubArray(vector<int>& nums) {
    auto freq = bykey::count_by(nums, [](int x) { return x; });
    size_t idx = 0;
    auto spans = bykey::minmax_by(
        nums,
        [](int x) { return x; },
        [&idx](int) { return static_cast<int>(idx++); },
        [&idx](int) { return static_cast<int>(idx - 1); }
    );

    size_t degree = 0;
    for (auto const& [_, count] : freq) degree = max(degree, count);

    int best = static_cast<int>(nums.size());
    for (auto const& [value, count] : freq) {
        if (count == degree) {
            auto mm = spans.at(value);
            best = min(best, mm.max - mm.min + 1);
        }
    }
    return best;
}

int main() {
    vector<int> nums{1, 2, 2, 3, 1, 4, 2};
    assert(findShortestSubArray(nums) == 6);
}
