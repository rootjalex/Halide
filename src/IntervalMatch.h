#ifndef HALIDE_INTERVAL_MATCH_H
#define HALIDE_INTERVAL_MATCH_H

#include "Interval.h"
#include "IRMatch.h"
#include "IROperator.h"
#include "Simplify.h" // can_prove()

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

    HALIDE_ALWAYS_INLINE
    Expr make(MatcherState &state, halide_type_t type_hint) const {
        // defined() should have been called.
        const Interval &interval = state.get_interval(i);
        return interval.min;
    }

    template<uint32_t bound, typename LambdaType>
    HALIDE_ALWAYS_INLINE
    bool defined(MatcherState &state, halide_type_t type_hint, LambdaType lambda) {
        // TODO: should be if constexpr when Halide switches to c++17.
        if (bound & binds) {
            // fetch the stored Interval's value.
            const Interval &interval = state.get_interval(i);
            return interval.has_lower_bound();
        } else {
            Wild<i> base_wild;
            Expr base = base_wild.make(state, type_hint);
            Interval interval = lambda(base);
            state.set_interval(i, interval);
            return interval.has_lower_bound();
        }
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

    HALIDE_ALWAYS_INLINE
    Expr make(MatcherState &state, halide_type_t type_hint) const {
        // defined() should have been called.
        const Interval &interval = state.get_interval(i);
        return interval.max;
    }

    template<uint32_t bound, typename LambdaType>
    HALIDE_ALWAYS_INLINE
    bool defined(MatcherState &state, halide_type_t type_hint, LambdaType lambda) {
        // TODO: should be if constexpr when Halide switches to c++17.
        if (bound & binds) {
            // fetch the stored Interval's value.
            const Interval &interval = state.get_interval(i);
            return interval.has_upper_bound();
        } else {
            static Wild<i> base_wild;
            Expr base = base_wild.make(state, type_hint);
            Interval interval = lambda(base);
            state.set_interval(i, interval);
            return interval.has_upper_bound();
        }
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
            if (after.template defined<0>(state, output_type, lambda)) {
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

    // TODO: add fuzzing tests?
    template<typename Before,
             typename After,
             typename Predicate,
             typename = typename enable_if_pattern<Before>::type,
             typename = typename enable_if_pattern<After>::type,
             typename = typename enable_if_pattern<Predicate>::type>
    HALIDE_ALWAYS_INLINE bool operator()(Before before, After after, Predicate pred) {
        static_assert((Before::binds & After::binds) == After::binds, "Rule result uses unbound values");
        static_assert((Before::binds & Predicate::binds) == Predicate::binds, "Rule predicate uses unbound values");
        static_assert(Before::canonical, "LHS of rewrite rule should be in canonical form");
        static_assert(After::canonical, "RHS of rewrite rule should be in canonical form");

        if (before.template match<0>(instance, state)) {
            if (pred.template defined<0>(state, output_type, lambda)) {
                Expr pred_expr = pred.make(state, output_type);
                // TODO: early-out for const predicates?
                if (can_prove(pred_expr)) {
                   if (after.template defined<0>(state, output_type, lambda)) {
                        build_replacement(after);
                        return true;
                    }
                }
            }
        }
        return false;
    }

    // TODO: add fuzzing tests?
    template<typename Before,
             typename Predicate,
             typename = typename enable_if_pattern<Before>::type,
             typename = typename enable_if_pattern<Predicate>::type>
    HALIDE_ALWAYS_INLINE bool operator()(Before before, const Expr &after, Predicate pred) noexcept {
        static_assert(Before::canonical, "LHS of rewrite rule should be in canonical form");
        static_assert((Before::binds & Predicate::binds) == Predicate::binds, "Rule predicate uses unbound values");
        if (before.template match<0>(instance, state)) {
            if (pred.template defined<0>(state, output_type, lambda)) {
                Expr pred_expr = pred.make(state, output_type);
                // TODO: early-out for const predicates?
                if (can_prove(pred_expr)) {
                    // after is already defined.
                    result = after;
                    return true;
                }
            }
        }
        return false;
    }

    // TODO: add fuzzing tests?
    template<typename Before,
             typename Predicate,
             typename = typename enable_if_pattern<Before>::type,
             typename = typename enable_if_pattern<Predicate>::type>
    HALIDE_ALWAYS_INLINE bool operator()(Before before, int64_t after, Predicate pred) noexcept {
        static_assert(Before::canonical, "LHS of rewrite rule should be in canonical form");
        static_assert((Before::binds & Predicate::binds) == Predicate::binds, "Rule predicate uses unbound values");
        if (before.template match<0>(instance, state)) {
            if (pred.template defined<0>(state, output_type, lambda)) {
                Expr pred_expr = pred.make(state, output_type);
                // TODO: early-out for const predicates?
                if (can_prove(pred_expr)) {
                    // after is already defined.
                    // TODO: this doesn't work for uint64_t.
                    result = make_const(output_type, after);
                    return true;
                }
            }
        }
        return false;
    }

    // TODO: allow pure boolean predicates
};

// TODO: rewriters for specific Exprs, possibly?
template<typename Instance, typename LambdaType,
         typename = typename enable_if_pattern<Instance>::type>
HALIDE_ALWAYS_INLINE
auto bounds_rewriter(Instance instance, halide_type_t output_type, LambdaType lambda) noexcept -> BoundsRewriter<decltype(pattern_arg(instance)), LambdaType> {
    return {pattern_arg(instance), output_type, lambda};
}

// Used for Predicates.
template<int i>
struct PointCheck {
    // TODO: is this necessary?
    struct pattern_tag {};

    constexpr static uint32_t binds = 1 << (i + 16);

    // This rule is a boolean-valued predicate. Bools have type UIntImm.
    constexpr static IRNodeType min_node_type = IRNodeType::UIntImm;
    constexpr static IRNodeType max_node_type = IRNodeType::UIntImm;
    constexpr static bool canonical = true;

    HALIDE_ALWAYS_INLINE
    Expr make(MatcherState &state, halide_type_t type_hint) const {
        // defined() should have been called.
        const Interval &interval = state.get_interval(i);
        return make_bool(interval.is_single_point());
    }

    template<uint32_t bound, typename LambdaType>
    HALIDE_ALWAYS_INLINE
    bool defined(MatcherState &state, halide_type_t type_hint, LambdaType lambda) {
        if ((bound & binds) == 0) {
            static Wild<i> base_wild;
            Expr base = base_wild.make(state, type_hint);
            Interval interval = lambda(base);
            state.set_interval(i, interval);
        }
        return true;
    }

    // TODO: should this be foldable?
    constexpr static bool foldable = false;
};

// Used for Predicates.
template<int i>
struct StrictPointCheck {
    // TODO: is this necessary?
    struct pattern_tag {};

    constexpr static uint32_t binds = 1 << (i + 16);

    // This rule is a boolean-valued predicate. Bools have type UIntImm.
    constexpr static IRNodeType min_node_type = IRNodeType::UIntImm;
    constexpr static IRNodeType max_node_type = IRNodeType::UIntImm;
    constexpr static bool canonical = true;

    const Expr *expr;

    StrictPointCheck(const Expr *_expr)
        : expr(_expr) {
    }

    HALIDE_ALWAYS_INLINE
    Expr make(MatcherState &state, halide_type_t type_hint) const {
        // defined() should have been called.
        const Interval &interval = state.get_interval(i);
        return make_bool(interval.is_single_point(*expr));
    }

    template<uint32_t bound, typename LambdaType>
    HALIDE_ALWAYS_INLINE
    bool defined(MatcherState &state, halide_type_t type_hint, LambdaType lambda) {
        if ((bound & binds) == 0) {
            static Wild<i> base_wild;
            Expr base = base_wild.make(state, type_hint);
            Interval interval = lambda(base);
            state.set_interval(i, interval);
        }
        return true;
    }

    // TODO: should this be foldable?
    constexpr static bool foldable = false;
};

template<int i>
HALIDE_ALWAYS_INLINE
PointCheck<i> is_single_point(WildInterval<i> interval) {
    return PointCheck<i>();
}

template<int i>
HALIDE_ALWAYS_INLINE
StrictPointCheck<i> is_single_point(WildInterval<i> interval, const Expr &e) {
    return StrictPointCheck<i>(&e);
}

}  // namespace IRMatcher
}  // namespace Internal
}  // namespace Halide

#endif // HALIDE_INTERVAL_MATCH_H
