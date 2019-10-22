
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

//[mitchell02_simple_queue
#include <boost/contract.hpp>
#include <boost/optional.hpp>
#include <vector>
#include <cassert>

template<typename T>
class simple_queue
    #define BASES private boost::contract::constructor_precondition< \
            simple_queue<T> >
    : BASES
{
    friend class boost::contract::access;

    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

    void invariant() const {
        BOOST_CONTRACT_ASSERT(count() >= 0); // Non-negative count.
    }

public:
    /* Creation */

    // Create empty queue.
    explicit simple_queue(int a_capacity) :
        boost::contract::constructor_precondition<simple_queue>([&] {
            BOOST_CONTRACT_ASSERT(a_capacity > 0); // Positive capacity.
        })
    {
        boost::contract::check c = boost::contract::constructor(this)
            .postcondition([&] {
                // Capacity set.
                BOOST_CONTRACT_ASSERT(capacity() == a_capacity);
                BOOST_CONTRACT_ASSERT(is_empty()); // Empty.
            })
        ;

        items_.reserve(a_capacity);
    }

    // Destroy queue.
    virtual ~simple_queue() {
        // Check invariants.
        boost::contract::check c = boost::contract::destructor(this);
    }

    /* Basic Queries */

    // Items in queue (in their order).
    // (Somewhat exposes implementation but allows to check more contracts.)
    std::vector<T> const& items() const {
        // Check invariants.
        boost::contract::check c = boost::contract::public_function(this);
        return items_;
    }

    // Max number of items queue can hold.
    int capacity() const {
        // Check invariants.
        boost::contract::check c = boost::contract::public_function(this);
        return items_.capacity();
    }

    /* Derived Queries */

    // Number of items.
    int count() const {
        int result;
        boost::contract::check c = boost::contract::public_function(this)
            .postcondition([&] {
                // Return items count.
                BOOST_CONTRACT_ASSERT(result == int(items().size()));
            })
        ;

        return result = items_.size();
    }

    // Item at head.
    T const& head() const {
        boost::optional<T const&> result;
        boost::contract::check c = boost::contract::public_function(this)
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(!is_empty()); // Not empty.
            })
            .postcondition([&] {
                // Return item on top.
                BOOST_CONTRACT_ASSERT(*result == items().at(0));
            })
        ;

        return *(result = items_.at(0));
    }

    // If queue contains no item.
    bool is_empty() const {
        bool result;
        boost::contract::check c = boost::contract::public_function(this)
            .postcondition([&] {
                // Consistent with count.
                BOOST_CONTRACT_ASSERT(result == (count() == 0));
            })
        ;

        return result = (items_.size() == 0);
    }

    // If queue has no room for another item.
    bool is_full() const {
        bool result;
        boost::contract::check c = boost::contract::public_function(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT( // Consistent with size and capacity.
                        result == (capacity() == int(items().size())));
            })
        ;

        return result = (items_.size() == items_.capacity());
    }

    /* Commands */

    // Remove head itme and shift all other items.
    void remove() {
        // Expensive all_equal postcond. and old_items copy might be skipped.
        boost::contract::old_ptr<std::vector<T> > old_items;
            #ifdef BOOST_CONTRACT_AUDIITS
                = BOOST_CONTRACT_OLDOF(items())
            #endif // Else, leave old pointer null...
        ;
        boost::contract::old_ptr<int> old_count = BOOST_CONTRACT_OLDOF(count());
        boost::contract::check c = boost::contract::public_function(this)
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(!is_empty()); // Not empty.
            })
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(count() == *old_count - 1); // Count dec.
                // ...following skipped #ifndef AUDITS.
                if(old_items) all_equal(items(), *old_items, /* shifted = */ 1);
            })
        ;
        
        items_.erase(items_.begin());
    }

    // Add item to tail.
    void put(T const& item) {
        // Expensive all_equal postcond. and old_items copy might be skipped.
        boost::contract::old_ptr<std::vector<T> > old_items;
            #ifdef BOOST_CONTRACT_AUDITS
                = BOOST_CONTRACT_OLDOF(items())
            #endif // Else, leave old pointer null...
        ;
        boost::contract::old_ptr<int> old_count = BOOST_CONTRACT_OLDOF(count());
        boost::contract::check c = boost::contract::public_function(this)
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(count() < capacity()); // Room for add.
            })
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(count() == *old_count + 1); // Count inc.
                // Second to last item.
                BOOST_CONTRACT_ASSERT(items().at(count() - 1) == item);
                // ...following skipped #ifndef AUDITS.
                if(old_items) all_equal(items(), *old_items);
            })
        ;
        
        items_.push_back(item);
    }

private:
    // Contract helper.
    static bool all_equal(std::vector<T> const& left,
            std::vector<T> const& right, unsigned offset = 0) {
        boost::contract::check c = boost::contract::function()
            .precondition([&] {
                // Correct offset.
                BOOST_CONTRACT_ASSERT(right.size() == left.size() + offset);
            })
        ;

        for(unsigned i = offset; i < right.size(); ++i) {
            if(left.at(i - offset) != right.at(i)) return false;
        }
        return true;
    }

    std::vector<T> items_;
};

int main() {
    simple_queue<int> q(10);
    q.put(123);
    q.put(456);

    assert(q.capacity() == 10);
    assert(q.head() == 123);

    assert(!q.is_empty());
    assert(!q.is_full());

    std::vector<int> const& items = q.items();
    assert(items.at(0) == 123);
    assert(items.at(1) == 456);
    
    q.remove();
    assert(q.count() == 1);

    return 0;
}
//]

