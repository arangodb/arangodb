#include <boost/detail/lightweight_test.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/include/vector.hpp>
#include <boost/fusion/include/make_vector.hpp>
#include <boost/fusion/include/equal_to.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/optional.hpp>
#include <string>

namespace x3 = boost::spirit::x3;

// just an `attr` with added type checker
template <typename Value, typename Expected>
struct checked_attr_parser : x3::attr_parser<Value>
{
    using base_t = x3::attr_parser<Value>;

    checked_attr_parser(Value const& value) : base_t(value) {}
    checked_attr_parser(Value&& value) : base_t(std::move(value)) {}

    template <typename Iterator, typename Context
      , typename RuleContext, typename Attribute>
    bool parse(Iterator& first, Iterator const& last
      , Context const& ctx, RuleContext& rctx, Attribute& attr_) const
    {
        static_assert(boost::is_same<Expected, Attribute>::value,
            "attribute type check failed");
        return base_t::parse(first, last, ctx, rctx, attr_);
    }
};

template <typename Expected, typename Value>
static inline checked_attr_parser<boost::decay_t<Value>, Expected>
checked_attr(Value&& value) { return { std::forward<Value>(value) }; }

// instantiate our type checker
// (checks attribute value just to be sure we are ok)
template <typename Value, typename Expr>
static void test_expr(Value const& v, Expr&& expr)
{
    char const* it = "";
    Value r;
    BOOST_TEST((x3::parse(it, it, std::forward<Expr>(expr), r)));
    BOOST_TEST((r == v));
}

template <typename Expr, typename Attribute>
static void gen_sequence(Attribute const& attribute, Expr&& expr)
{
    test_expr(attribute, expr);
    test_expr(attribute, expr >> x3::eps);
}

template <typename Expected, typename... ExpectedTail, typename Attribute, typename Expr, typename Value, typename... Tail>
static void gen_sequence(Attribute const& attribute, Expr&& expr, Value const& v, Tail const&... tail)
{
    gen_sequence<ExpectedTail...>(attribute, expr >> checked_attr<Expected>(v), tail...);
    gen_sequence<ExpectedTail...>(attribute, expr >> x3::eps >> checked_attr<Expected>(v), tail...);
    gen_sequence<ExpectedTail...>(attribute, expr >> (x3::eps >> checked_attr<Expected>(v)), tail...);
}

template <typename Expected, typename... ExpectedTail, typename Attribute, typename Value, typename... Tail>
static void gen_sequence_tests(Attribute const& attribute, Value const& v, Tail const&... tail)
{
    gen_sequence<ExpectedTail...>(attribute, checked_attr<Expected>(v), tail...);
    gen_sequence<ExpectedTail...>(attribute, x3::eps >> checked_attr<Expected>(v), tail...);
}

template <typename Expected, typename Value>
static void gen_single_item_tests(Value const& v)
{
    Expected attribute(v);
    gen_sequence(attribute, checked_attr<Expected>(v));
    gen_sequence(attribute, x3::eps >> checked_attr<Expected>(v));
}

template <typename Expected, typename... ExpectedTail, typename Value, typename... Tail>
static void gen_single_item_tests(Value const& v, Tail const&... tail)
{
    gen_single_item_tests<Expected>(v);
    gen_single_item_tests<ExpectedTail...>(tail...);
}

template <typename... Expected, typename... Values>
static void gen_tests(Values const&... values)
{
    gen_single_item_tests<Expected...>(values...);

    boost::fusion::vector<Expected...> attribute = boost::fusion::make_vector(values...);
    gen_sequence_tests<Expected...>(attribute, values...);
}

template <typename... Attributes>
void make_test(Attributes const&... attrs)
{
    // I would like to place all of this in a single call
    // but it requires tremendous amount of heap to compile
    gen_tests<Attributes...>(attrs...);
    gen_tests<
        boost::optional<Attributes>...
      , boost::fusion::vector<Attributes>...
    >(attrs..., attrs...);
    gen_tests<
        boost::optional<boost::fusion::vector<Attributes>>...
      , boost::fusion::vector<boost::optional<Attributes>>...
    >(boost::fusion::vector<Attributes>(attrs)..., attrs...);
}

int main()
{
    make_test<int, std::string>(123, "hello");
    return boost::report_errors();
}
