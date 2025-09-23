// include/by-key/by_key.hpp
#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <ranges>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace bykey {

namespace detail {

template <class Map>
constexpr void try_reserve(Map& m, std::size_t n) {
    if constexpr (requires(Map& mm, std::size_t size) { mm.reserve(size); }) {
        if (n) m.reserve(n);
    }
}

template <class F>
struct pipeable {
    F func;

    template <std::ranges::range R>
    auto operator()(R&& r) const {
        return func(std::forward<R>(r));
    }

    template <std::ranges::range R>
    friend auto operator|(R&& r, pipeable const& p) {
        return p.func(std::forward<R>(r));
    }
};

template <class T>
struct sum_traits {
    auto identity() const -> T { return T{}; }

    template <class Value>
    void combine(T& acc, Value&& v) const {
        acc += static_cast<T>(std::forward<Value>(v));
    }
};

template <class Traits, class Acc>
concept has_finalize = requires(Traits const& traits, Acc const& acc) {
    traits.finalize(acc);
};

template <class Range>
constexpr auto size_hint(Range&& r, std::size_t expected) {
    if (expected) return expected;
    if constexpr (std::ranges::sized_range<Range>) {
        return static_cast<std::size_t>(std::ranges::size(r));
    } else {
        return std::size_t{0};
    }
}

template <class Value, class Order>
struct extrema_state {
    Value min_value;
    Order min_order;
    Value max_value;
    Order max_order;
};

template <class Acc, class BinaryOp>
struct basic_transform_traits {
    using acc_type = std::decay_t<Acc>;
    using op_type  = std::decay_t<BinaryOp>;

    acc_type init;
    op_type combine_op;

    auto identity() const { return init; }

    template <class Value>
    void combine(acc_type& acc, Value&& v) const {
        acc = combine_op(acc, std::forward<Value>(v));
    }
};

template <class Range, class KeyProj, class ValProj, class Traits>
auto transform_reduce_by_impl(Range&& r,
                              KeyProj key,
                              ValProj value,
                              Traits traits,
                              std::size_t expected_unique) {
    using Ref = std::ranges::range_reference_t<Range>;
    using K   = std::decay_t<std::invoke_result_t<KeyProj&, Ref>>;
    using Acc = std::decay_t<decltype(traits.identity())>;

    std::unordered_map<K, Acc> accs;
    try_reserve(accs, size_hint(r, expected_unique));

    auto key_proj    = std::move(key);
    auto value_proj  = std::move(value);
    auto traits_copy = std::move(traits);

    for (auto&& x : r) {
        auto key_value = key_proj(x);
        auto value_copy = value_proj(x);
        auto [it, inserted] = accs.try_emplace(key_value, traits_copy.identity());
        traits_copy.combine(it->second, std::move(value_copy));
    }

    if constexpr (has_finalize<Traits, Acc>) {
        using Result = std::decay_t<decltype(traits_copy.finalize(std::declval<Acc const&>()))>;
        std::unordered_map<K, Result> out;
        try_reserve(out, accs.size());
        for (auto& [k, acc] : accs) {
            out.emplace(k, traits_copy.finalize(acc));
        }
        return out;
    } else {
        return accs;
    }
}

} // namespace detail

template <class Value>
struct extrema_result {
    Value min;
    Value max;
};

template <class Value>
struct partition_result {
    std::vector<Value> falses;
    std::vector<Value> trues;
};

// ---- core ---------------------------------------------------------------

template <std::ranges::input_range R, class KeyProj>
auto count_by(R&& r, KeyProj key, std::size_t expected_unique = 0) {
    using Ref  = std::ranges::range_reference_t<R>;
    using K    = std::decay_t<std::invoke_result_t<KeyProj&, Ref>>;
    std::unordered_map<K, std::size_t> freq;

    detail::try_reserve(freq, detail::size_hint(r, expected_unique));

    auto key_proj = std::move(key);
    for (auto&& x : r) ++freq[key_proj(x)];
    return freq;
}

template <std::ranges::input_range R, class KeyProj, class ValProj, class Map>
auto index_by_into(R&& r, KeyProj key, ValProj val, Map m, bool overwrite = true) {
    using Ref = std::ranges::range_reference_t<R>;
    using K   = std::decay_t<std::invoke_result_t<KeyProj&, Ref>>;
    using V   = std::decay_t<std::invoke_result_t<ValProj&, Ref>>;

    if constexpr (std::ranges::sized_range<R>) {
        detail::try_reserve(m, std::ranges::size(r));
    }

    auto key_proj = std::move(key);
    auto val_proj = std::move(val);

    for (auto&& x : r) {
        K key_value = key_proj(x);
        V val_value = val_proj(x);
        if (overwrite) {
            m[key_value] = std::move(val_value);
        } else {
            m.emplace(std::move(key_value), std::move(val_value));
        }
    }
    return m;
}

template <std::ranges::input_range R, class KeyProj, class ValProj>
auto index_by(R&& r, KeyProj key, ValProj val, bool overwrite = true) {
    using Ref = std::ranges::range_reference_t<R>;
    using K   = std::decay_t<std::invoke_result_t<KeyProj&, Ref>>;
    using V   = std::decay_t<std::invoke_result_t<ValProj&, Ref>>;
    return index_by_into(std::forward<R>(r), std::move(key), std::move(val), std::unordered_map<K, V>{}, overwrite);
}

template <std::ranges::input_range R, class KeyProj, class ValProj, class Acc, class BinOp>
auto group_reduce_by(R&& r, KeyProj key, ValProj value, Acc init, BinOp op,
                     std::size_t expected_unique = 0) {
    using Ref = std::ranges::range_reference_t<R>;
    using K   = std::decay_t<std::invoke_result_t<KeyProj&, Ref>>;

    std::unordered_map<K, Acc> m;
    detail::try_reserve(m, detail::size_hint(r, expected_unique));

    auto key_proj = std::move(key);
    auto val_proj = std::move(value);
    auto op_fn    = std::move(op);

    for (auto&& x : r) {
        auto key_value = key_proj(x);
        auto value_copy = val_proj(x);
        auto [it, inserted] = m.try_emplace(key_value, init);
        op_fn(it->second, std::move(value_copy));
    }
    return m;
}

// ---- convenience algorithms --------------------------------------------

template <std::ranges::input_range R, class KeyProj, class ValProj, class Map>
auto group_by_into(R&& r, KeyProj key, ValProj value, Map m, std::size_t expected_unique = 0) {
    using Ref = std::ranges::range_reference_t<R>;
    using Bucket = typename Map::mapped_type;
    static_assert(requires { typename Bucket::value_type; },
                  "group_by_into expects Map::mapped_type to expose value_type");
    using V = std::decay_t<std::invoke_result_t<ValProj&, Ref>>;
    static_assert(std::is_convertible_v<V, typename Bucket::value_type>,
                  "Value projection must produce values convertible to bucket type");
    static_assert(requires(Bucket& b, typename Bucket::value_type v) { b.push_back(std::move(v)); },
                  "Bucket type must support push_back");

    detail::try_reserve(m, detail::size_hint(r, expected_unique));

    auto key_proj = std::move(key);
    auto val_proj = std::move(value);

    for (auto&& x : r) {
        auto key_value = key_proj(x);
        auto val_value = val_proj(x);
        m[key_value].push_back(std::move(val_value));
    }
    return m;
}

template <std::ranges::input_range R, class KeyProj, class ValProj = std::identity>
auto group_by(R&& r, KeyProj key, ValProj value = {}, std::size_t expected_unique = 0) {
    using Ref = std::ranges::range_reference_t<R>;
    using K   = std::decay_t<std::invoke_result_t<KeyProj&, Ref>>;
    using V   = std::decay_t<std::invoke_result_t<ValProj&, Ref>>;
    return group_by_into(std::forward<R>(r), std::move(key), std::move(value), std::unordered_map<K, std::vector<V>>{}, expected_unique);
}

template <std::ranges::input_range R, class KeyProj, class ValProj, class Traits>
auto transform_reduce_by(R&& r, KeyProj key, ValProj value, Traits traits, std::size_t expected_unique = 0) {
    return detail::transform_reduce_by_impl(std::forward<R>(r), std::move(key), std::move(value), std::move(traits), expected_unique);
}

template <std::ranges::input_range R, class KeyProj, class ValProj, class Acc, class BinaryOp>
auto transform_reduce_by(R&& r, KeyProj key, ValProj value, Acc init, BinaryOp combine, std::size_t expected_unique = 0) {
    auto traits = detail::basic_transform_traits<Acc, BinaryOp>{std::move(init), std::move(combine)};
    return detail::transform_reduce_by_impl(std::forward<R>(r), std::move(key), std::move(value), std::move(traits), expected_unique);
}

template <std::ranges::input_range R, class KeyProj, class ValProj>
auto accumulate_by(R&& r, KeyProj key, ValProj value, std::size_t expected_unique = 0) {
    using Ref = std::ranges::range_reference_t<R>;
    using V   = std::decay_t<std::invoke_result_t<ValProj&, Ref>>;
    return transform_reduce_by(std::forward<R>(r), std::move(key), std::move(value), detail::sum_traits<V>{}, expected_unique);
}

template <std::ranges::input_range R, class KeyProj, class ValProj, class T>
auto accumulate_by(R&& r, KeyProj key, ValProj value, T init, std::size_t expected_unique = 0) {
    return transform_reduce_by(std::forward<R>(r), std::move(key), std::move(value), std::move(init), std::plus<>{}, expected_unique);
}

template <std::ranges::input_range R, class KeyProj, class ValProj, class OrderProj = ValProj, class Compare = std::ranges::less>
auto extrema_by(R&& r,
                KeyProj key,
                ValProj value,
                OrderProj order = {},
                Compare comp = {},
                std::size_t expected_unique = 0) {
    using Ref = std::ranges::range_reference_t<R>;
    using K   = std::decay_t<std::invoke_result_t<KeyProj&, Ref>>;
    using V   = std::decay_t<std::invoke_result_t<ValProj&, Ref>>;
    using O   = std::decay_t<std::invoke_result_t<OrderProj&, Ref>>;

    std::unordered_map<K, detail::extrema_state<V, O>> states;
    detail::try_reserve(states, detail::size_hint(r, expected_unique));

    auto key_proj   = std::move(key);
    auto value_proj = std::move(value);
    auto order_proj = std::move(order);
    auto compare    = std::move(comp);

    for (auto&& x : r) {
        auto key_value = key_proj(x);
        auto order_value = order_proj(x);
        auto val_value = value_proj(x);
        auto [it, inserted] = states.try_emplace(
            key_value,
            detail::extrema_state<V, O>{val_value, order_value, val_value, order_value}
        );
        if (!inserted) {
            auto& st = it->second;
            if (compare(order_value, st.min_order)) {
                st.min_order = order_value;
                st.min_value = val_value;
            }
            if (compare(st.max_order, order_value)) {
                st.max_order = order_value;
                st.max_value = val_value;
            }
        }
    }

    std::unordered_map<K, extrema_result<V>> out;
    detail::try_reserve(out, states.size());
    for (auto& [k, st] : states) {
        out.emplace(k, extrema_result<V>{st.min_value, st.max_value});
    }
    return out;
}

template <std::ranges::input_range R, class KeyProj, class ValProj, class OrderProj = ValProj, class Compare = std::ranges::less>
auto minmax_by(R&& r,
               KeyProj key,
               ValProj value,
               OrderProj order = {},
               Compare comp = {},
               std::size_t expected_unique = 0) {
    return extrema_by(std::forward<R>(r), std::move(key), std::move(value), std::move(order), std::move(comp), expected_unique);
}

template <class Map>
auto to_sorted_pairs(Map const& m, auto cmp) {
    using P = std::pair<typename Map::key_type, typename Map::mapped_type>;
    std::vector<P> out;
    out.reserve(m.size());
    for (auto const& kv : m) out.emplace_back(kv.first, kv.second);
    std::ranges::sort(out, cmp);
    return out;
}

template <class Map, class Cmp>
auto top_k(Map const& m, std::size_t k, Cmp cmp) {
    auto v = to_sorted_pairs(m, cmp);
    if (v.size() > k) v.resize(k);
    return v;
}

template <class Map>
auto top_k_by_value(Map const& m, std::size_t k) {
    return top_k(m, k, [](auto const& a, auto const& b) {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });
}

template <class Map>
auto top_k_by_key(Map const& m, std::size_t k) {
    return top_k(m, k, [](auto const& a, auto const& b) {
        if (a.first != b.first) return a.first < b.first;
        return a.second > b.second;
    });
}

template <class Map>
auto bottom_k_by_value(Map const& m, std::size_t k) {
    return top_k(m, k, [](auto const& a, auto const& b) {
        if (a.second != b.second) return a.second < b.second;
        return a.first < b.first;
    });
}

template <std::ranges::input_range R, class Pred, class ValProj = std::identity>
auto partition_by(R&& r, Pred pred, ValProj value = {}) {
    using Ref = std::ranges::range_reference_t<R>;
    using V   = std::decay_t<std::invoke_result_t<ValProj&, Ref>>;
    partition_result<V> out;

    auto pred_proj = std::move(pred);
    auto value_proj = std::move(value);

    for (auto&& x : r) {
        bool is_true = static_cast<bool>(pred_proj(x));
        auto val_value = value_proj(x);
        if (is_true) out.trues.push_back(std::move(val_value));
        else         out.falses.push_back(std::move(val_value));
    }
    return out;
}

// ---- pipeline adaptors -------------------------------------------------

namespace adaptors {

template <class KeyProj>
auto count(KeyProj key, std::size_t expected_unique = 0) {
    return detail::pipeable{[key = std::move(key), expected_unique](auto&& range) {
        return count_by(std::forward<decltype(range)>(range), key, expected_unique);
    }};
}

template <class KeyProj, class ValProj = std::identity>
auto group(KeyProj key, ValProj value = {}, std::size_t expected_unique = 0) {
    return detail::pipeable{[key = std::move(key), value = std::move(value), expected_unique](auto&& range) {
        return group_by(std::forward<decltype(range)>(range), key, value, expected_unique);
    }};
}

template <class KeyProj, class ValProj>
auto accumulate(KeyProj key, ValProj value, std::size_t expected_unique = 0) {
    return detail::pipeable{[key = std::move(key), value = std::move(value), expected_unique](auto&& range) {
        return accumulate_by(std::forward<decltype(range)>(range), key, value, expected_unique);
    }};
}

template <class KeyProj, class ValProj, class Traits>
auto transform_reduce(KeyProj key, ValProj value, Traits traits, std::size_t expected_unique = 0) {
    return detail::pipeable{[key = std::move(key), value = std::move(value), traits = std::move(traits), expected_unique](auto&& range) {
        return transform_reduce_by(std::forward<decltype(range)>(range), key, value, traits, expected_unique);
    }};
}

template <class KeyProj, class ValProj, class OrderProj = ValProj, class Compare = std::ranges::less>
auto extrema(KeyProj key, ValProj value, OrderProj order = {}, Compare comp = {}, std::size_t expected_unique = 0) {
    return detail::pipeable{[key = std::move(key), value = std::move(value), order = std::move(order), comp = std::move(comp), expected_unique](auto&& range) {
        return extrema_by(std::forward<decltype(range)>(range), key, value, order, comp, expected_unique);
    }};
}

template <class Pred, class ValProj = std::identity>
auto partition(Pred pred, ValProj value = {}) {
    return detail::pipeable{[pred = std::move(pred), value = std::move(value)](auto&& range) {
        return partition_by(std::forward<decltype(range)>(range), pred, value);
    }};
}

} // namespace adaptors

} // namespace bykey
