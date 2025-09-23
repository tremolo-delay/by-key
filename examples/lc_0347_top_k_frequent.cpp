#include <cassert>
#include <cstddef>
#include <vector>

#include <by-key/by_key.hpp>

using namespace std;

vector<int> topKFrequent(vector<int>& nums, int k) {
    auto freq = bykey::count_by(nums, [](int x) { return x; });
    auto top  = bykey::top_k_by_value(freq, static_cast<size_t>(k));

    vector<int> out;
    out.reserve(top.size());
    for (auto const& [value, _] : top) out.push_back(value);
    return out;
}

int main() {
    vector<int> nums{1, 1, 1, 2, 2, 3};
    auto result = topKFrequent(nums, 2);
    assert(result.size() == 2);
}
