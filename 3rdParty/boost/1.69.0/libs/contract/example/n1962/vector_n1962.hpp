
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

//[n1962_vector_n1962
// Extra spaces, newlines, etc. for visual alignment with this library code.

#include <boost/algorithm/cxx11/all_of.hpp>
#include <boost/type_traits/has_equal_to.hpp>
#include <boost/next_prior.hpp>
#include <vector>
#include <iterator>
#include <memory>





























template< class T, class Allocator = std::allocator<T> >
class vector {
    
    
    invariant {
        empty() == (size() == 0);
        std::distance(begin(), end()) == int(size());
        std::distance(rbegin(), rend()) == int(size());
        size() <= capacity();
        capacity() <= max_size();
    }

public:
    typedef typename std::vector<T, Allocator>::allocator_type allocator_type;
    typedef typename std::vector<T, Allocator>::pointer pointer;
    typedef typename std::vector<T, Allocator>::const_pointer const_pointer;
    typedef typename std::vector<T, Allocator>::reference reference;
    typedef typename std::vector<T, Allocator>::const_reference const_reference;
    typedef typename std::vector<T, Allocator>::value_type value_type;
    typedef typename std::vector<T, Allocator>::iterator iterator;
    typedef typename std::vector<T, Allocator>::const_iterator const_iterator;
    typedef typename std::vector<T, Allocator>::size_type size_type;
    typedef typename std::vector<T, Allocator>::difference_type difference_type;
    typedef typename std::vector<T, Allocator>::reverse_iterator
            reverse_iterator;
    typedef typename std::vector<T, Allocator>::const_reverse_iterator
            const_reverse_iterator;

    vector()
        postcondition {
            empty();
        }
        : vect_()
    {}
    
    
    explicit vector(Allocator const& alloc)
        postcondition { 
            empty();
            get_allocator() == alloc;
        }
        : vect_(alloc)
    {}

    
    explicit vector(size_type count)
        postcondition { 
            size() == count;
            if constexpr(boost::has_equal_to<T>::value) {
                boost::algorithm::all_of_equal(begin(), end(), T());
            }
        }
        : vect_(count)
    {}

    
    
    
    vector(size_type count, T const& value)
        postcondition {
            size() == count;
            if constexpr(boost::has_equal_to<T>::value) {
                boost::algorithm::all_of_equal(begin(), end(), value);
            }
        }
        : vect_(count, value)
    {}

    
    
    
    
    vector(size_type count, T const& value, Allocator const& alloc)
        postcondition {
            size() == count;
            if constexpr(boost::has_equal_to<T>::value) {
                boost::algorithm::all_of_equal(begin(), end(), value);
            }
            get_allocator() == alloc;
        }
        : vect_(count, value, alloc)
    {}


    
    
    
    
    template<typename InputIter>
    vector(InputIter first, InputIter last)
        postcondition {
            std::distance(first, last) == int(size());
        }
        : vect_(first, last)
    {}


    
    template<typename InputIter>
    vector(InputIter first, InputIter last, Allocator const& alloc)
        postcondition {
            std::distance(first, last) == int(size());
            get_allocator() == alloc;
        }
        : vect_(first, last, alloc)
    {}


    
    
    /* implicit */ vector(vector const& other)
        postcondition {
            if constexpr(boost::has_equal_to<T>::value) {
                *this == other;
            }
        }
        : vect_(other.vect_)
    {}


    
    
    
    vector& operator=(vector const& other)
        postcondition(result) {
            if constexpr(boost::has_equal_to<T>::value) {
                *this == other;
                result == *this;
            }
        }
    {
        if(this != &other) vect_ = other.vect_;
        return *this;
    }


    
    
   


    
    
    
    
    
    virtual ~vector() {}


    
    
    void reserve(size_type count)
        precondition {
            count < max_size();
        }
        postcondition {
            capacity() >= count;
        }
    {
        vect_.reserve(count);
    }


    
    size_type capacity() const
        postcondition(result) {
            result >= size();
        }
    {
        return vect_.capacity();
    }


    
    
    iterator begin()
        postcondition {
            if(empty()) result == end();
        }
    {
        return vect_.begin();
    }


    
    
    const_iterator begin() const
        postcondition(result) {
            if(empty()) result == end();
        }
    {
        return vect_.begin();
    }


    
    
    iterator end() {
        return vect_.end();
    }



    const_iterator end() const {
        return vect_.end();
    }


    
    reverse_iterator rbegin()
        postcondition(result) {
            if(empty()) result == rend();
        }
    {
        return vect_.rbegin();
    }


    
    
    const_reverse_iterator rbegin() const
        postcondition(result) {
            if(empty()) result == rend();
        }
    {
        return vect_.rbegin();
    }


    
    
    reverse_iterator rend() {
        return vect_.rend();
    }


    
    const_reverse_iterator rend() const {
        return vect_.rend();
    }



    void resize(size_type count, T const& value = T())
        postcondition {
            size() == count;
            if constexpr(boost::has_equal_to<T>::value) {
                if(count > oldof(size())) {
                    boost::algorithm::all_of_equal(begin() + oldof(size()),
                            end(), value);
                }
            }
        }
    {
        vect_.resize(count, value);
    }



    
    
    
    
    size_type size() const
        postcondition(result) {
            result <= capacity();
        }
    {
        return vect_.size();
    }


    
    
    size_type max_size() const
        postcondition(result) {
            result >= capacity();
        }
    {
        return vect_.max_size();
    }


    
    
    bool empty() const
        postcondition(result) {
            result == (size() == 0);
        }
    {
        return vect_.empty();
    }


    
    
    Alloctor get_allocator() const {
        return vect_.get_allocator();
    }


    
    reference at(size_type index) {
        // No precondition (throw out_of_range for invalid index).
        return vect_.at(index);
    }


    const_reference at(size_type index) const {
        // No precondition (throw out_of_range for invalid index).
        return vect_.at(index);
    }


    reference operator[](size_type index)
        precondition {
            index < size();
        }
    {
        return vect_[index];
    }


    
    const_reference operator[](size_type index) const
        precondition {
            index < size();
        }
    {
        return vect_[index];
    }


    
    reference front()
        precondition {
            !empty();
        }
    {
        return vect_.front();
    }


    
    const_reference front() const
        precondition {
            !empty();
        }
    {
        return vect_.front();
    }


    
    reference back()
        precondition {
            !empty();
        }
    {
        return vect_.back();
    }


    
    const_reference back() const
        precondition {
            !empty();
        }
    {
        return vect_.back();
    }


    
    void push_back(T const& value)
        precondition {
            size() < max_size();
        }
        postcondition {
            size() == oldof(size()) + 1;
            capacity() >= oldof(capacity())
            if constexpr(boost::has_equal_to<T>::value) {
                back() == value;
            }
        }
    {
        vect_.push_back(value);
    }


    
    
    
    
    
    
    
    
    void pop_back()
        precondition {
            !empty();
        }
        postcondition {
            size() == oldof(size()) - 1;
        }
    {
        vect_.pop_back();
    }


    
    
    
    template<typename InputIter>
    void assign(InputIter first, InputIter last)
        // Precondition: [begin(), end()) does not contain [first, last).
        postcondition {
            std::distance(first, last) == int(size());
        }
    {
        vect_.assign(first, last);
    }


    
    
    
    
    
    void assign(size_type count, T const& vallue)
        precondition {
            count <= max_size();
        }
        postcondition {
            if constexpr(boost::has_equal_to<T>::value) {
                boost::algorithm::all_of_equal(begin(), end(), value);
            }
        }
    {
        vect_.assign(count, value);
    }


    
    
    
    
    iterator insert(iterator where, T const& value)
        precondition {
            size() < max_size();
        }
        postcondition(result) {
            size() == oldof(size()) + 1;
            capacity() >= oldof(capacity());
            if constexpr(boost::has_equal_to<T>::value) {
                *result == value;
            }
            //  if(capacity() > oldof(capacity()))
            //      [begin(), end()) is invalid
            //  else
            //      [where, end()) is invalid
        }
    {
        return vect_.insert(where, value);
    }


    
    
    
    
    
    
    
    
    
    
    void insert(iterator where, size_type count, T const& value)
        precondition {
            size() + count < max_size();
        }
        postcondition {
            size() == oldof(size()) + count;
            capacity() >= oldof(capacity());
            if(capacity() == oldof(capacity())) {
                if constexpr(boost::has_equal_to<T>::value) {
                    boost::algorithm::all_of_equal(boost::prior(oldof(where)),
                            boost::prior(oldof(where)) + count, value);
                }
                // [where, end()) is invalid
            }
            // else [begin(), end()) is invalid
        }
    {
        vect_.insert(where, count, value);
    }


    
    
    
    
    
    
    
    
    
    
    
    template<typename InputIter>
    void insert(iterator where, Iterator first, Iterator last)
        precondition {
            size() + std::distance(first, last) < max_size();
            // [first, last) is not contained in [begin(), end())
        }
        postcondition {
            size() == oldof(size()) + std::distance(first, last);
            capacity() >= oldof(capacity());
            if(capacity() == oldof(capacity())) {
                if constexpr(boost::has_equal_to<T>::value) {
                    boost::algorithm::all_of_equal(first, last, oldof(where));
                }
                // [where, end()) is invalid
            }
            // else [begin(), end()) is invalid
        }
    {
        vect_.insert(where, first, last);
    }


    
    
    
    
    
    
    
    
    
    
    
    
    iterator erase(iterator where)
        precondition {
            !empty();
            where != end();
        }
        postcondition(result) {
            size() == oldod size() - 1;
            if(empty()) result == end();
            // [where, end()) is invalid
        }
    {
        return vect_.erase(where);
    }


    
    
    
    
    iterator erase(iterator first, iterator last)
        precondition {
            size() >= std::distance(first, lasst);
        }
        postcondition(result) {
            size() == oldof(size()) - std::distance(first, last);
            if(empty()) result == end();
            // [first, last) is invalid
        }
    {
        return vect_.erase(first, last);
    }

    
    
    
    
    
    
    void clear()
        postcondition {
            empty();
        }
    {
        vect_.clear();
    }



    void swap(vector& other)
        precondition {
            get_allocator() == other.get_allocator();
        }
        postcondition {
            if constexpr(boost::has_equal_to<T>::value) {
                *this == oldof(other);
                other == oldof(*this);
            }
        }
    {
        vect_.swap(other);
    }
















    
    
    friend bool operator==(vector const& left, vector const& right) {
        // Cannot check class invariants for left and right objects.
        return left.vect_ == right.vect_;
    }





private:
    std::vector<T, Allocator> vect_;
};












































// End.
//]

