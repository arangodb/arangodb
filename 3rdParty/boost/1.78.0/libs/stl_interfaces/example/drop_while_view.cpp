// Copyright (C) 2019 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/stl_interfaces/view_interface.hpp>

#include <algorithm>
#include <vector>

#include <cassert>


//[ all_view
// A subrange is simply an iterator-sentinel pair.  This one is a bit simpler
// than the one in std::ranges; its missing a bunch of constructors, prev(),
// next(), and advance().
template<typename Iterator, typename Sentinel>
struct subrange
    : boost::stl_interfaces::view_interface<subrange<Iterator, Sentinel>>
{
    subrange() = default;
    constexpr subrange(Iterator it, Sentinel s) : first_(it), last_(s) {}

    constexpr auto begin() const { return first_; }
    constexpr auto end() const { return last_; }

private:
    Iterator first_;
    Sentinel last_;
};

// std::view::all() returns one of several types, depending on what you pass
// it.  Here, we're keeping it simple; all() always returns a subrange.
template<typename Range>
auto all(Range && range)
{
    return subrange<decltype(range.begin()), decltype(range.end())>(
        range.begin(), range.end());
}

// A template alias that denotes the type of all(r) for some Range r.
template<typename Range>
using all_view = decltype(all(std::declval<Range>()));
//]

//[ drop_while_view_template

// Perhaps its clear now why we defined subrange, all(), etc. above.
// drop_while_view contains a view data member.  If we just took any old range
// that was passed to drop_while_view's constructor, we'd copy the range
// itself, which may be a std::vector.  So, we want to make a view out of
// whatever Range we're given so that this copy of an owning range does not
// happen.
template<typename Range, typename Pred>
struct drop_while_view
    : boost::stl_interfaces::view_interface<drop_while_view<Range, Pred>>
{
    using base_type = all_view<Range>;

    drop_while_view() = default;

    constexpr drop_while_view(Range & base, Pred pred) :
        base_(all(base)),
        pred_(std::move(pred))
    {}

    constexpr base_type base() const { return base_; }
    constexpr Pred const & pred() const noexcept { return pred_; }

    // A more robust implementation should probably cache the value computed
    // by this function, so that subsequent calls can just return the cached
    // iterator.
    constexpr auto begin()
    {
        // We're forced to write this out as a raw loop, since no
        // std::-namespace algorithms accept a sentinel.
        auto first = base_.begin();
        auto const last = base_.end();
        for (; first != last; ++first) {
            if (!pred_(*first))
                break;
        }
        return first;
    }

    constexpr auto end() { return base_.end(); }

private:
    base_type base_;
    Pred pred_;
};

// Since this is a C++14 and later library, we're not using CTAD; we therefore
// need a make-function.
template<typename Range, typename Pred>
auto make_drop_while_view(Range & base, Pred pred)
{
    return drop_while_view<Range, Pred>(base, std::move(pred));
}
//]


int main()
{
    //[ drop_while_view_usage
    std::vector<int> const ints = {2, 4, 3, 4, 5, 6};

    // all() returns a subrange, which is a view type containing ints.begin()
    // and ints.end().
    auto all_ints = all(ints);

    // This works using just the used-defined members of subrange: begin() and
    // end().
    assert(
        std::equal(all_ints.begin(), all_ints.end(), ints.begin(), ints.end()));

    // These are available because subrange is derived from view_interface.
    assert(all_ints[2] == 3);
    assert(all_ints.size() == 6u);

    auto even = [](int x) { return x % 2 == 0; };
    auto ints_after_even_prefix = make_drop_while_view(ints, even);

    // Available via begin()/end()...
    assert(std::equal(
        ints_after_even_prefix.begin(),
        ints_after_even_prefix.end(),
        ints.begin() + 2,
        ints.end()));

    // ... and via view_interface.
    assert(!ints_after_even_prefix.empty());
    assert(ints_after_even_prefix[2] == 5);
    assert(ints_after_even_prefix.back() == 6);
    //]
}
