#ifndef HALIDE_INTERVAL_MATCH_H
#define HALIDE_INTERVAL_MATCH_H

#include "Interval.h"
#include "IRMatch.h"

namespace Halide {
namespace Internal {
namespace IRMatcher {

template<int i>
struct WildLowerBound {
    struct pattern_tag {};

    constexpr static uint32_t binds = 1 << (i + 16);

    constexpr static IRNodeType min_node_type = IRNodeType::IntImm;
    constexpr static IRNodeType max_node_type = StrongestExprNodeType;
    constexpr static bool canonical = true;

    Expr min_value = 0;

    HALIDE_ALWAYS_INLINE
    Expr make(MatcherState &state, halide_type_t type_hint) const {
        // defined() should have been called.
        return min_value;
    }

    template<typename LambdaType>
    HALIDE_ALWAYS_INLINE
    bool defined(MatcherState &state, halide_type_t type_hint, LambdaType lambda) {
        Wild<i> base_wild;
        Expr base = base_wild.make(state, type_hint);
        Interval interval = lambda(base);
        min_value = interval.min;
        return interval.has_lower_bound();
    }

    // TODO: should this be foldable?
    constexpr static bool foldable = false;
};

template<int i>
struct WildUpperBound {
    struct pattern_tag {};

    constexpr static uint32_t binds = 1 << (i + 16);

    constexpr static IRNodeType min_node_type = IRNodeType::IntImm;
    constexpr static IRNodeType max_node_type = StrongestExprNodeType;
    constexpr static bool canonical = true;

    Expr max_value = 0;

    HALIDE_ALWAYS_INLINE
    Expr make(MatcherState &state, halide_type_t type_hint) const {
        // defined() should have been called.
        return max_value;
    }

    template<typename LambdaType>
    HALIDE_ALWAYS_INLINE
    bool defined(MatcherState &state, halide_type_t type_hint, LambdaType lambda) {
        Wild<i> base_wild;
        Expr base = base_wild.make(state, type_hint);
        Interval interval = lambda(base);
        max_value = interval.max;
        return interval.has_upper_bound();
    }

    // TODO: should this be foldable?
    constexpr static bool foldable = false;
};

template<int i>
struct WildInterval {
    struct pattern_tag {};

    // WildConst is [0, 6), Wild is [8, 14), WildInterval is [16, 22)
    constexpr static uint32_t binds = 1 << (i + 16);

    constexpr static IRNodeType min_node_type = IRNodeType::IntImm;
    constexpr static IRNodeType max_node_type = StrongestExprNodeType;
    constexpr static bool canonical = true;

    template<uint32_t bound>
    HALIDE_ALWAYS_INLINE bool match(const BaseExprNode &e, MatcherState &state) const noexcept {
        // This is the same as struct Wild's match().
        if (bound & binds) {
            return equal(*state.get_binding(i), e);
        }
        state.set_binding(i, e);
        return true;
    }

    // TODO: should this be foldable?
    constexpr static bool foldable = false;

    WildLowerBound<i> min;
    WildUpperBound<i> max;
};

template<int i>
inline std::ostream &operator<<(std::ostream &s, const WildInterval<i> &e) {
    s << "wild_interval<" << i << ">";
    return s;
}

template<typename Instance, typename LambdaType>
struct BoundsRewriter {
    Instance instance;
    LambdaType lambda;
    Expr result;
    MatcherState state;
    // TODO: do we need wildcard_type or validate boolean?
    halide_type_t output_type;

    HALIDE_ALWAYS_INLINE
    BoundsRewriter(Instance &&_instance, halide_type_t _output_type, LambdaType _lambda)
        : instance(std::forward<Instance>(_instance)), lambda(_lambda), output_type(_output_type) {
    }

    template<typename After>
    HALIDE_NEVER_INLINE void build_replacement(After after) {
        result = after.make(state, output_type);
    }

    // TODO: add fuzzing tests?
    template<typename Before,
            typename After,
            typename = typename enable_if_pattern<Before>::type,
            typename = typename enable_if_pattern<After>::type>
    HALIDE_ALWAYS_INLINE
    bool operator()(Before before, After after) {
        static_assert((Before::binds & After::binds) == After::binds, "Rule result uses unbound values");
        static_assert(Before::canonical, "LHS of rewrite rule should be in canonical form");
        static_assert(After::canonical, "RHS of rewrite rule should be in canonical form");

        if (before.template match<0>(instance, state)) {
            // Checks if mins/maxs referenced are defined and fills in needed values.
            if (after.defined(state, output_type, lambda)) {
                build_replacement(after);
                return true;
            }
        }
        // TODO: add debugging statements for no match vs. ill-defined result.
        return false;
    }

    // TODO: add fuzzing tests?
    template<typename Before,
             typename = typename enable_if_pattern<Before>::type>
    HALIDE_ALWAYS_INLINE bool operator()(Before before, const Expr &after) noexcept {
        static_assert(Before::canonical, "LHS of rewrite rule should be in canonical form");
        if (before.template match<0>(instance, state)) {
            // after is already defined.
            result = after;
            return true;
        } else {
            return false;
        }
    }

    // TODO: add fuzzing tests?
    template<typename Before,
             typename = typename enable_if_pattern<Before>::type>
    HALIDE_ALWAYS_INLINE bool operator()(Before before, int64_t after) noexcept {
        static_assert(Before::canonical, "LHS of rewrite rule should be in canonical form");
        if (before.template match<0>(instance, state)) {
            // after is already defined.
            // TODO: this doesn't work for uint64_t.
            result = make_const(output_type, after);
            return true;
        } else {
            return false;
        }
    }

    // TODO: Predicates.
};

// TODO: rewriters for specific Exprs, possibly?
template<typename Instance, typename LambdaType,
         typename = typename enable_if_pattern<Instance>::type>
HALIDE_ALWAYS_INLINE
auto bounds_rewriter(Instance instance, halide_type_t output_type, LambdaType lambda) noexcept -> BoundsRewriter<decltype(pattern_arg(instance)), LambdaType> {
    return {pattern_arg(instance), output_type, lambda};
}

}  // namespace IRMatcher
}  // namespace Internal
}  // namespace Halide

#endif // HALIDE_INTERVAL_MATCH_H
