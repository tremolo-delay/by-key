#include <algorithm>
#include <cassert>
#include <vector>

#include <by-key/by_key.hpp>

using namespace std;

vector<int> arrayRankTransform(vector<int>& arr) {
    vector<int> uniq = arr;
    sort(uniq.begin(), uniq.end());
    uniq.erase(unique(uniq.begin(), uniq.end()), uniq.end());

    auto rank = bykey::index_by(
        uniq,
        [](int x) { return x; },
        [next = 1](int) mutable { return next++; }
    );

    vector<int> out;
    out.reserve(arr.size());
    for (int x : arr) out.push_back(rank.at(x));
    return out;
}

int main() {
    vector<int> arr{40, 10, 20, 30};
    auto rank = arrayRankTransform(arr);
    assert((rank == vector<int>{4, 1, 2, 3}));
}
