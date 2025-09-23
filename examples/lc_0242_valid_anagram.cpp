#include <cassert>
#include <string>

#include <by-key/by_key.hpp>

using namespace std;

bool isAnagram(string s, string t) {
    return bykey::count_by(s, [](char c) { return c; }) ==
           bykey::count_by(t, [](char c) { return c; });
}

int main() {
    assert(isAnagram("anagram", "nagaram"));
    assert(!isAnagram("rat", "car"));
}
