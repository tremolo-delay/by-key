#include <cassert>
#include <vector>

#include <by-key/by_key.hpp>

using namespace std;

vector<int> intersect(vector<int>& nums1, vector<int>& nums2) {
    auto counts = bykey::count_by(nums1, [](int x) { return x; });

    vector<int> out;
    out.reserve(min(nums1.size(), nums2.size()));
    for (int x : nums2) {
        auto it = counts.find(x);
        if (it != counts.end() && it->second > 0) {
            out.push_back(x);
            --it->second;
        }
    }
    return out;
}

int main() {
    vector<int> a{1, 2, 2, 1};
    vector<int> b{2, 2};
    auto v = intersect(a, b);
    assert(v.size() == 2 && v[0] == 2 && v[1] == 2);
}
