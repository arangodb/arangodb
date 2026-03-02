// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//[ segmented_vector
#include <boost/text/segmented_vector.hpp>

#include <iostream>
#include <vector>


// This is a general-purpose undo/redo system.  It consists of a vector of
// states, which is mostly used as a stack, but which we can also iterate
// over.  We also have an iterator, which always points to a valid element of
// our state vector, indicating the current state.

// Undo can then be implemented by simply decrementing the current-state
// iterator; redo is performed by incrementing it.

// Copying our entire working state is potentially very expensive, of course.
// The data sharing inherent to segmented_vector's operation means that
// copying or mutating an entire segmented_vector mostly shares data, making
// those program state copies cheap.

// This gets a new undo state, by erasing any pending redos (more recent
// states), then copying the most recent state and returning an iterator to
// the copy.
template<typename T>
typename std::vector<boost::text::segmented_vector<T>>::iterator new_undo_state(
    std::vector<boost::text::segmented_vector<T>> & history,
    typename std::vector<boost::text::segmented_vector<T>>::iterator it)
{
    history.erase(std::next(it), history.end());
    return history.insert(history.end(), history.back());
}

// Undo just decrements the current-state iterator, except that undos that
// would underflow are ignored.
template<typename T>
typename std::vector<boost::text::segmented_vector<T>>::iterator undo(
    std::vector<boost::text::segmented_vector<T>> const & history,
    typename std::vector<boost::text::segmented_vector<T>>::iterator it)
{
    if (it != history.begin())
        --it;
    return it;
}

// Like undo, in reverse.
template<typename T>
typename std::vector<boost::text::segmented_vector<T>>::iterator redo(
    std::vector<boost::text::segmented_vector<T>> const & history,
    typename std::vector<boost::text::segmented_vector<T>>::iterator it)
{
    if (std::next(it) != history.end())
        ++it;
    return it;
}

// Gets a new state to work on by calling new_undo_state(), and then applies
// an arbitrary mutation to the new state.
template<typename T, typename Func>
typename std::vector<boost::text::segmented_vector<T>>::iterator mutate_state(
    std::vector<boost::text::segmented_vector<T>> & history,
    typename std::vector<boost::text::segmented_vector<T>>::iterator it,
    Func && f)
{
    it = new_undo_state(history, it);
    *it = std::forward<Func>(f)(*it);
    return it;
}

// Print the history, latest to oldest.
template<typename T>
void dump_history(
    std::vector<boost::text::segmented_vector<T>> const & history,
    typename std::vector<boost::text::segmented_vector<T>>::iterator it)
{
    std::cout << "========================================\n";
    int i = 0;
    for (auto first = history.begin(), last = history.end(); last-- != first;) {
        if (last == it)
            std::cout << " -> ";
        else
            std::cout << "    ";
        std::cout << "history " << i++ << ": [ ";
        for (auto & x : *last) {
            std::cout << x << ' ';
        }
        std::cout << "]\n";
    }
    std::cout << "\n";
}


int main ()
{
    using state_type = boost::text::segmented_vector<int>;

    // Here, we push a single value onto the end of the segmented_vector that
    // represents our current state.  This is a cheap operation even if the
    // current state contains billions of elements.
    int i = 0;
    auto push_next = [&i](state_type state) {
        state.insert(state.end(), i++);
        return state;
    };

    // Start with one empty history entry.
    std::vector<state_type> undo_history(1);
    std::vector<state_type>::iterator current_state_it = undo_history.begin();

    // Shows current state as "[ ]".
    dump_history(undo_history, current_state_it);

    // Copies the initial empty state; pushes a 0 onto the end of the copy;
    // returns an iterator to the result.
    current_state_it = mutate_state(undo_history, current_state_it, push_next);

    // Pushes a 1 onto the end of another copy.
    current_state_it = mutate_state(undo_history, current_state_it, push_next);

    // Shows current state as "[ 0 1 ]".
    dump_history(undo_history, current_state_it);

    // Conditionally decrements a pointer (pointing back to the initial empty
    // state).
    current_state_it = undo(undo_history, current_state_it);

    // Shows current state as "[ 0 ]", with one more recent state "[ 0 1 ]".
    dump_history(undo_history, current_state_it);

    // Conditionally increments a pointer (pointing back to the result of the first operation).
    current_state_it = redo(undo_history, current_state_it);

    // Shows current state as "[ 0 1 ]" again.
    dump_history(undo_history, current_state_it);

    // Unodes one state, pushes a 2 onto the end of another copy.  The push
    // clears any history that is after the current one.
    current_state_it = undo(undo_history, current_state_it);
    current_state_it = mutate_state(undo_history, current_state_it, push_next);

    // Shows current state as "[ 0 2 ]", with no more recent states.
    dump_history(undo_history, current_state_it);
}
//]
