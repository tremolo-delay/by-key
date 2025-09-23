#include <algorithm>
#include <cassert>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

#include <by-key/by_key.hpp>

using namespace std;

vector<vector<string>> groupAnagrams(vector<string>& strs) {
    auto groups = bykey::group_by(
        strs,
        [](string s) {
            ranges::sort(s);
            return s;
        },
        [](const string& s) { return s; }
    );

    vector<vector<string>> out;
    out.reserve(groups.size());
    for (auto& [_, bucket] : groups) {
        out.push_back(std::move(bucket));
    }
    return out;
}

int main() {
    vector<string> words{"eat", "tea", "tan", "ate", "nat", "bat"};
    auto groups = groupAnagrams(words);
    size_t total = 0;
    for (auto const& bucket : groups) total += bucket.size();
    assert(total == words.size());
}
