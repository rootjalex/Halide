#ifndef HALIDE_INTERVAL_MATCH_H
#define HALIDE_INTERVAL_MATCH_H

#include "IRMatch.h"
#include "IROperator.h"
#include "Interval.h"
#include "Simplify.h"  // can_prove()

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
    HALIDE_ALWAYS_INLINE bool defined(MatcherState &state, halide_type_t type_hint, LambdaType lambda) {
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
    HALIDE_ALWAYS_INLINE bool defined(MatcherState &state, halide_type_t type_hint, LambdaType lambda) {
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

// Produces Interval from (possibly symbolic) values.
template<typename A, typename B>
struct IntervalMatcher {
    A min;
    B max;

    constexpr static uint32_t binds = bindings<A>::mask | bindings<B>::mask;

    Interval make_interval(MatcherState &state, halide_type_t type_hint) const noexcept {
        Expr e_min = min.make(state, type_hint);
        Expr e_max = max.make(state, type_hint);
        return Halide::Internal::Interval(std::move(e_min), std::move(e_max));
    }

    template<uint32_t bound, typename LambdaType>
    HALIDE_ALWAYS_INLINE bool defined(MatcherState &state, halide_type_t type_hint, LambdaType lambda) {
        return min.template defined<bound>(state, type_hint, lambda) &&
               max.template defined<bound | bindings<A>::mask>(state, type_hint, lambda);
    }
};

template<typename A, typename B>
HALIDE_ALWAYS_INLINE auto MakeInterval(A a, B b) noexcept -> IntervalMatcher<decltype(pattern_arg(a)), decltype(pattern_arg(b))> {
    return {pattern_arg(a), pattern_arg(b)};
}

template<int i>
inline std::ostream &operator<<(std::ostream &s, const WildInterval<i> &e) {
    s << "wild_interval<" << i << ">";
    return s;
}

template<typename Instance, typename LambdaType>
struct BoundsRewriter {
    Instance instance;
    LambdaType lambda;
    // Used when the return type is an Expr.
    Expr result;
    // Used when the returned type is an Interval.
    Halide::Internal::Interval interval_result;
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

    template<int i>
    HALIDE_NEVER_INLINE void build_interval(WildInterval<i> after) {
        interval_result.min = after.min.make(state, output_type);
        interval_result.max = after.max.make(state, output_type);
    }

    template<typename A, typename B>
    HALIDE_NEVER_INLINE void build_interval(IntervalMatcher<A, B> after) {
        interval_result = after.make_interval(state, output_type);
    }

    // TODO: add fuzzing tests?
    template<typename Before,
             typename After,
             typename = typename enable_if_pattern<Before>::type,
             typename = typename enable_if_pattern<After>::type>
    HALIDE_ALWAYS_INLINE bool operator()(Before before, After after) {
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
                if (evaluate_nonconst_predicate(pred, state, output_type)) {
                    // TODO: early-out for const predicates?
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
             int i,
             typename Predicate,
             typename = typename enable_if_pattern<Before>::type,
             typename = typename enable_if_pattern<Predicate>::type>
    HALIDE_ALWAYS_INLINE bool operator()(Before before, WildInterval<i> after, Predicate pred) {
        // TODO: static assert that i is in the proper range.
        static_assert((Before::binds & Predicate::binds) == Predicate::binds, "Rule predicate uses unbound values");
        static_assert(Before::canonical, "LHS of rewrite rule should be in canonical form");

        if (before.template match<0>(instance, state)) {
            if (pred.template defined<0>(state, output_type, lambda)) {
                if (evaluate_nonconst_predicate(pred, state, output_type)) {
                    // TODO: early-out for const predicates?
                    if (after.min.template defined<0>(state, output_type, lambda) &&
                        after.max.template defined<0>(state, output_type, lambda)) {
                        build_interval(after);
                        return true;
                    }
                }
            }
        }
        return false;
    }

    // TODO: add fuzzing tests?
    template<typename Before,
             typename A,
             typename B,
             typename Predicate,
             typename = typename enable_if_pattern<Before>::type,
             typename = typename enable_if_pattern<Predicate>::type>
    HALIDE_ALWAYS_INLINE bool operator()(Before before, IntervalMatcher<A, B> after, Predicate pred) {
        static_assert((Before::binds & IntervalMatcher<A, B>::binds) == IntervalMatcher<A, B>::binds, "Rule result uses unbound values");
        static_assert((Before::binds & Predicate::binds) == Predicate::binds, "Rule predicate uses unbound values");
        static_assert(Before::canonical, "LHS of rewrite rule should be in canonical form");

        if (before.template match<0>(instance, state)) {
            if (pred.template defined<0>(state, output_type, lambda)) {
                if (evaluate_nonconst_predicate(pred, state, output_type)) {
                    // TODO: early-out for const predicates?
                    if (after.template defined<0>(state, output_type, lambda)) {
                        build_interval(after);
                        return true;
                    }
                }
            }
        }
        return false;
    }


    // TODO: add fuzzing tests?
    template<typename Before,
             typename A,
             typename B,
             typename = typename enable_if_pattern<Before>::type>
    HALIDE_ALWAYS_INLINE bool operator()(Before before, IntervalMatcher<A, B> after) {
        static_assert((Before::binds & IntervalMatcher<A, B>::binds) == IntervalMatcher<A, B>::binds, "Rule result uses unbound values");
        static_assert(Before::canonical, "LHS of rewrite rule should be in canonical form");

        if (before.template match<0>(instance, state)) {
            if (after.template defined<0>(state, output_type, lambda)) {
                build_interval(after);
                return true;
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
                if (evaluate_nonconst_predicate(pred, state, output_type)) {
                    // TODO: early-out for const predicates?
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
                if (evaluate_nonconst_predicate(pred, state, output_type)) {
                    // TODO: early-out for const predicates?
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
HALIDE_ALWAYS_INLINE auto bounds_rewriter(Instance instance, halide_type_t output_type, LambdaType lambda) noexcept -> BoundsRewriter<decltype(pattern_arg(instance)), LambdaType> {
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
    HALIDE_ALWAYS_INLINE bool defined(MatcherState &state, halide_type_t type_hint, LambdaType lambda) {
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
    HALIDE_ALWAYS_INLINE bool defined(MatcherState &state, halide_type_t type_hint, LambdaType lambda) {
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
struct BoundCheck {
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
        return make_bool(interval.is_bounded());
    }

    template<uint32_t bound, typename LambdaType>
    HALIDE_ALWAYS_INLINE bool defined(MatcherState &state, halide_type_t type_hint, LambdaType lambda) {
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
PointCheck<i>
    is_single_point(WildInterval<i> interval) {
    return PointCheck<i>();
}

template<int i>
HALIDE_ALWAYS_INLINE
StrictPointCheck<i>
    is_single_point(WildInterval<i> interval, const Expr &e) {
    return StrictPointCheck<i>(&e);
}

template<int i>
HALIDE_ALWAYS_INLINE
BoundCheck<i> is_bounded(WildInterval<i> interval) {
    return BoundCheck<i>();
}

// wrapper used as an anti-Predicate rule.
template<typename C>
struct Anti {
    struct pattern_tag {};

    C cond;

    constexpr static uint32_t binds = bindings<C>::mask;

    Anti(C _cond)
        : cond(_cond) {
    }

    template<uint32_t bound, typename LambdaType>
    HALIDE_ALWAYS_INLINE bool defined(MatcherState &state, halide_type_t type_hint, LambdaType lambda) {
        // defer to conditional.
        return cond.template defined<bound>(state, type_hint, lambda);
    }
};

template<typename Predicate>
HALIDE_ALWAYS_INLINE bool evaluate_nonconst_predicate(Anti<Predicate> &p, MatcherState &state, halide_type_t output_type) {
    Expr pred_expr = p.cond.make(state, output_type);
    return !can_prove(pred_expr);
}

template<typename Predicate>
HALIDE_ALWAYS_INLINE bool evaluate_nonconst_predicate(Predicate &p, MatcherState &state, halide_type_t output_type) {
    Expr pred_expr = p.make(state, output_type);
    return can_prove(pred_expr);
}

template<typename C>
HALIDE_ALWAYS_INLINE Anti<C> anti(C cond) noexcept {
    return Anti<C>(cond);
}

// Used for Predicates.
template<typename A>
struct IsPosConst {
    // TODO: is this necessary?
    struct pattern_tag {};

    constexpr static uint32_t binds = bindings<A>::mask;

    // This rule is a boolean-valued predicate. Bools have type UIntImm.
    constexpr static IRNodeType min_node_type = IRNodeType::UIntImm;
    constexpr static IRNodeType max_node_type = IRNodeType::UIntImm;
    constexpr static bool canonical = true;

    A a;

    HALIDE_ALWAYS_INLINE
    Expr make(MatcherState &state, halide_type_t type_hint) const {
        // defined() should have been called.
        const Expr expr = a.make(state, type_hint);
        return make_bool(is_positive_const(expr));
    }

    template<uint32_t bound, typename LambdaType>
    HALIDE_ALWAYS_INLINE bool defined(MatcherState &state, halide_type_t type_hint, LambdaType lambda) {
        return a.template defined<bound>(state, type_hint, lambda);
    }

    // TODO: should this be foldable?
    constexpr static bool foldable = false;
};

template<typename A>
HALIDE_ALWAYS_INLINE auto is_pos_const(A a) noexcept -> IsPosConst<decltype(pattern_arg(a))> {
    return {pattern_arg(a)};
}

}  // namespace IRMatcher
}  // namespace Internal
}  // namespace Halide

#endif  // HALIDE_INTERVAL_MATCH_H
