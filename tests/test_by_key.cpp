#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <ranges>
#include <algorithm>
#include <unordered_map>
#include <map>
#include "by-key/by_key.hpp"

TEST(ByKey, CountByIntegers) {
    std::vector<int> a{1,2,2,3,1,4};
    auto freq = bykey::count_by(a, [](int x){ return x; });
    EXPECT_EQ(freq.at(1), 2);
    EXPECT_EQ(freq.at(2), 2);
    EXPECT_EQ(freq.at(3), 1);
    EXPECT_EQ(freq.at(4), 1);
}

TEST(ByKey, IndexByLastIndex) {
    std::vector<int> a{1,2,2,3,1,4};
    auto last_idx = bykey::index_by(a,
        [](int x){ return x; },
        [i=std::size_t{0}](int) mutable { return i++; },
        /*overwrite=*/true);
    EXPECT_EQ(last_idx.at(2), 2);
    EXPECT_EQ(last_idx.at(4), 5);
}

TEST(ByKey, GroupReduceAnagrams) {
    std::vector<std::string> words{"eat","tea","tan","ate","nat","bat"};
    auto groups = bykey::group_reduce_by(
        words,
        [](std::string s){ std::ranges::sort(s); return s; }, // key: sorted letters
        [](const std::string& s){ return s; },                // value: original word
        std::vector<std::string>{},
        [](auto& acc, std::string s){ acc.push_back(std::move(s)); });
    EXPECT_EQ(groups.at("aet").size(), 3); // eat, tea, ate
    EXPECT_EQ(groups.at("ant").size(), 2); // tan, nat
    EXPECT_EQ(groups.at("abt").size(), 1); // bat
}

TEST(ByKey, TopKByValueDeterministic) {
    std::vector<int> xs{1,2,2,3,1,4,6,6,6};
    auto freq = bykey::count_by(xs, [](int x){ return x; });
    auto top2 = bykey::top_k_by_value(freq, 2);
    ASSERT_EQ(top2.size(), 2u);
    EXPECT_EQ(top2[0].first, 6); // most frequent
    // second place is {1 or 2}, tie broken by key ascending in helper
    EXPECT_TRUE(top2[1].first == 1 || top2[1].first == 2);
}

TEST(ByKey, GroupByConvenience) {
    std::vector<std::string> words{"ant","anchor","bat","ball","apple","coral"};
    auto grouped = bykey::group_by(words, [](const std::string& s){ return s.front(); });
    ASSERT_EQ(grouped.at('a').size(), 3u);
    ASSERT_EQ(grouped.at('b').size(), 2u);

    std::unordered_map<char, std::vector<std::string>> reuse;
    reuse['z'] = {"zzz"};
    auto reused = bykey::group_by_into(
        words,
        [](const std::string& s){ return s.back(); },
        [](const std::string& s){ return s; },
        std::move(reuse));
    EXPECT_EQ(reused.at('z').size(), 1u);
    EXPECT_EQ(reused.at('t').size(), 2u);
    EXPECT_EQ(reused.at('l').size(), 2u);
}

TEST(ByKey, AccumulateBySum) {
    struct Score { std::string team; int points; };
    std::vector<Score> scores{{"red", 3}, {"blue", 2}, {"red", 5}, {"blue", 4}, {"red", -1}};

    auto totals = bykey::accumulate_by(
        scores,
        [](const Score& s){ return s.team; },
        [](const Score& s){ return s.points; });

    EXPECT_EQ(totals.at("red"), 7);
    EXPECT_EQ(totals.at("blue"), 6);

    auto biased = bykey::accumulate_by(
        scores,
        [](const Score& s){ return s.team; },
        [](const Score& s){ return s.points; },
        10);
    EXPECT_EQ(biased.at("red"), 17);
}

TEST(ByKey, TransformReduceWithTraits) {
    struct AvgTraits {
        struct state { double sum = 0.0; int count = 0; };

        auto identity() const { return state{}; }

        void combine(state& s, int value) const {
            s.sum += value;
            ++s.count;
        }

        double finalize(state const& s) const {
            return s.count ? s.sum / s.count : 0.0;
        }
    } traits;

    struct Sample { std::string bucket; int v; };
    std::vector<Sample> samples{{"a", 2}, {"b", 10}, {"a", 6}, {"b", 2}, {"a", 4}};

    auto averages = bykey::transform_reduce_by(
        samples,
        [](const Sample& s){ return s.bucket; },
        [](const Sample& s){ return s.v; },
        traits);

    EXPECT_DOUBLE_EQ(averages.at("a"), 4.0);
    EXPECT_DOUBLE_EQ(averages.at("b"), 6.0);
}

TEST(ByKey, ExtremaByKeepsPerKeyMinMax) {
    struct Reading { std::string sensor; int value; int timestamp; };
    std::vector<Reading> readings{
        {"alpha", 10, 100},
        {"beta", 5, 80},
        {"alpha", 4, 90},
        {"beta", 12, 200},
        {"alpha", 15, 300}
    };

    auto extrema = bykey::extrema_by(
        readings,
        [](const Reading& r){ return r.sensor; },
        [](const Reading& r){ return r; },
        [](const Reading& r){ return r.timestamp; });

    auto minmax = bykey::minmax_by(
        readings,
        [](const Reading& r){ return r.sensor; },
        [](const Reading& r){ return r.value; });

    EXPECT_EQ(extrema.at("alpha").min.timestamp, 90);
    EXPECT_EQ(extrema.at("alpha").max.timestamp, 300);
    EXPECT_EQ(extrema.at("beta").min.value, 5);
    EXPECT_EQ(extrema.at("beta").max.value, 12);

    EXPECT_EQ(minmax.at("alpha").min, 4);
    EXPECT_EQ(minmax.at("alpha").max, 15);
}

TEST(ByKey, ExtremaByHandlesMoveBeforeOrder) {
    struct Entry {
        std::string key;
        std::string payload;
    };

    std::vector<Entry> entries{
        {"alpha", "zzz"},
        {"alpha", "xx"},
        {"alpha", "longer"},
        {"beta", "solo"}
    };

    auto extrema = bykey::extrema_by(
        entries,
        [](const Entry& e){ return e.key; },
        [](Entry& e){ return std::move(e.payload); },
        [](const Entry& e){ return e.payload.size(); }
    );

    auto alpha = extrema.at("alpha");
    EXPECT_EQ(alpha.min, "xx");
    EXPECT_EQ(alpha.max, "longer");

    auto beta = extrema.at("beta");
    EXPECT_EQ(beta.min, "solo");
    EXPECT_EQ(beta.max, "solo");
}

TEST(ByKey, TopAndBottomKHelpers) {
    std::unordered_map<int, int> freq{{1, 4}, {2, 2}, {3, 9}, {4, 1}};

    auto by_key = bykey::top_k_by_key(freq, 3);
    ASSERT_EQ(by_key.size(), 3u);
    EXPECT_EQ(by_key.front().first, 1);
    EXPECT_EQ(by_key.back().first, 3);

    auto smallest = bykey::bottom_k_by_value(freq, 2);
    ASSERT_EQ(smallest.size(), 2u);
    EXPECT_EQ(smallest[0].first, 4);
    EXPECT_EQ(smallest[1].first, 2);

    auto lowest_twins = bykey::top_k(freq, 2, [](auto const& a, auto const& b){
        if (a.second != b.second) return a.second < b.second;
        return a.first < b.first;
    });
    EXPECT_EQ(lowest_twins.front().first, 4);
    EXPECT_EQ(lowest_twins.back().first, 2);
}

TEST(ByKey, PartitionByBooleanPredicate) {
    std::vector<int> values{1,2,3,4,5,6};
    auto partitions = bykey::partition_by(values, [](int v){ return v % 2 == 0; });

    ASSERT_EQ(partitions.trues.size(), 3u);
    EXPECT_EQ(partitions.trues[0], 2);
    EXPECT_EQ(partitions.trues[2], 6);

    ASSERT_EQ(partitions.falses.size(), 3u);
    EXPECT_EQ(partitions.falses[0], 1);
    EXPECT_EQ(partitions.falses[2], 5);
}

TEST(ByKey, PipelineAdaptorsCompose) {
    std::vector<int> numbers{1,1,2,3,5,8,13};

    auto remainders = numbers | bykey::adaptors::count([](int x){ return x % 3; });
    EXPECT_EQ(remainders.at(1), 3u);
    EXPECT_EQ(remainders.at(2), 3u);
    EXPECT_EQ(remainders.at(0), 1u);

    auto grouped = numbers | bykey::adaptors::group([](int x){ return x % 2; });
    EXPECT_EQ(grouped.at(0).size(), 2u);
    EXPECT_EQ(grouped.at(1).size(), 5u);

    auto sums = numbers | bykey::adaptors::accumulate(
        [](int x){ return x % 2; },
        [](int x){ return x; });
    EXPECT_EQ(sums.at(0), 10);
    EXPECT_EQ(sums.at(1), 23);

    struct AvgTraits {
        struct state { int sum = 0; int count = 0; };
        auto identity() const { return state{}; }
        void combine(state& s, int v) const { s.sum += v; ++s.count; }
        double finalize(state const& s) const { return s.count ? static_cast<double>(s.sum) / s.count : 0.0; }
    } traits;

    auto avg_by_parity = numbers | bykey::adaptors::transform_reduce(
        [](int x){ return x % 2; },
        [](int x){ return x; },
        traits);
    EXPECT_DOUBLE_EQ(avg_by_parity.at(0), 5.0);
    EXPECT_NEAR(avg_by_parity.at(1), 4.6, 1e-9);

    auto partitioned = numbers | bykey::adaptors::partition([](int x){ return x < 5; });
    ASSERT_EQ(partitioned.trues.size(), 4u);
    EXPECT_EQ(partitioned.trues[0], 1);
    ASSERT_EQ(partitioned.falses.size(), 3u);
    EXPECT_EQ(partitioned.falses.back(), 13);
}

TEST(ByKey, PartitionByEvaluatesPredicateBeforeMove) {
    std::vector<std::string> words{"on", "stop", "cab", "a", "longword"};

    auto partitions = bykey::partition_by(
        words,
        [](const std::string& s){ return s.size() > 2; },
        [](std::string& s){ return std::move(s); }
    );

    EXPECT_EQ(partitions.trues, (std::vector<std::string>{"stop", "cab", "longword"}));
    EXPECT_EQ(partitions.falses, (std::vector<std::string>{"on", "a"}));
}

TEST(Examples, LC0049_GroupAnagrams) {
    std::vector<std::string> words{"eat","tea","tan","ate","nat","bat"};
    auto groups = bykey::group_by(
        words,
        [](std::string s){ std::ranges::sort(s); return s; },
        [](const std::string& s){ return s; }
    );

    std::map<std::string, std::vector<std::string>> actual;
    size_t total = 0;
    for (auto& [signature, bucket] : groups) {
        std::sort(bucket.begin(), bucket.end());
        total += bucket.size();
        actual.emplace(signature, bucket);
    }
    EXPECT_EQ(total, words.size());

    std::map<std::string, std::vector<std::string>> expected{
        {"aet", {"ate", "eat", "tea"}},
        {"ant", {"nat", "tan"}},
        {"abt", {"bat"}}
    };
    EXPECT_EQ(actual, expected);
}

TEST(Examples, LC0347_TopKFrequent) {
    std::vector<int> nums{1,1,1,2,2,3};
    auto freq = bykey::count_by(nums, [](int x){ return x; });
    auto top  = bykey::top_k_by_value(freq, 2);

    std::vector<int> values;
    for (auto const& [value, _] : top) values.push_back(value);
    std::sort(values.begin(), values.end());
    EXPECT_EQ(values, (std::vector<int>{1,2}));
}

TEST(Examples, LC0697_DegreeOfArray) {
    std::vector<int> nums{1,2,2,3,1,4,2};
    auto freq = bykey::count_by(nums, [](int x){ return x; });

    std::size_t idx = 0;
    auto spans = bykey::minmax_by(
        nums,
        [](int x){ return x; },
        [&idx](int){ return static_cast<int>(idx++); },
        [&idx](int){ return static_cast<int>(idx - 1); }
    );

    std::size_t degree = 0;
    for (auto const& [_, count] : freq) degree = std::max(degree, count);

    int best = static_cast<int>(nums.size());
    for (auto const& [value, count] : freq) {
        if (count == degree) {
            auto mm = spans.at(value);
            best = std::min(best, mm.max - mm.min + 1);
        }
    }

    EXPECT_EQ(best, 6);
}

TEST(Examples, LC0350_IntersectionWithMultiplicity) {
    std::vector<int> a{1,2,2,1};
    std::vector<int> b{2,2};
    auto counts = bykey::count_by(a, [](int x){ return x; });

    std::vector<int> out;
    for (int x : b) {
        auto it = counts.find(x);
        if (it != counts.end() && it->second > 0) {
            out.push_back(x);
            --it->second;
        }
    }

    EXPECT_EQ(out, (std::vector<int>{2,2}));
}

TEST(Examples, LC0242_ValidAnagram) {
    EXPECT_TRUE(bykey::count_by(std::string{"anagram"}, [](char c){ return c; }) ==
                bykey::count_by(std::string{"nagaram"}, [](char c){ return c; }));
    EXPECT_FALSE(bykey::count_by(std::string{"rat"}, [](char c){ return c; }) ==
                 bykey::count_by(std::string{"car"}, [](char c){ return c; }));
}

TEST(Examples, LC1331_RankTransform) {
    std::vector<int> arr{40, 10, 20, 30};
    std::vector<int> uniq = arr;
    std::sort(uniq.begin(), uniq.end());
    uniq.erase(std::unique(uniq.begin(), uniq.end()), uniq.end());

    auto ranks = bykey::index_by(
        uniq,
        [](int x){ return x; },
        [next = 1](int) mutable { return next++; }
    );

    std::vector<int> result;
    result.reserve(arr.size());
    for (int x : arr) result.push_back(ranks.at(x));

    EXPECT_EQ(result, (std::vector<int>{4,1,2,3}));
}
