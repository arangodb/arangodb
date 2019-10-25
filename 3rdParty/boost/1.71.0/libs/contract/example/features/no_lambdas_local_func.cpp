
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <boost/contract.hpp>
#include <boost/local_function.hpp>
#include <boost/bind.hpp>
#include <cassert>

class iarray :
        private boost::contract::constructor_precondition<iarray> {
public:
    static void static_invariant() {
        BOOST_CONTRACT_ASSERT(instances() >= 0);
    }

    void invariant() const {
        BOOST_CONTRACT_ASSERT(size() <= capacity());
    }

    static void constructor_pre(unsigned const max, unsigned const count) {
        BOOST_CONTRACT_ASSERT(count <= max);
    }
    explicit iarray(unsigned max, unsigned count = 0) :
        boost::contract::constructor_precondition<iarray>(boost::bind(
                &iarray::constructor_pre, max, count)),
        values_(new int[max]),
        capacity_(max)
    {
        boost::contract::old_ptr<int> old_instances;
        void BOOST_LOCAL_FUNCTION(bind& old_instances) {
            old_instances = BOOST_CONTRACT_OLDOF(iarray::instances());
        } BOOST_LOCAL_FUNCTION_NAME(old)
        void BOOST_LOCAL_FUNCTION(const bind this_, const bind& count,
                const bind& old_instances) { 
            BOOST_CONTRACT_ASSERT(this_->size() == count);
            BOOST_CONTRACT_ASSERT(this_->instances() == *old_instances + 1);
        } BOOST_LOCAL_FUNCTION_NAME(post)
        boost::contract::check c = boost::contract::constructor(this)
                .old(old).postcondition(post);

        for(unsigned i = 0; i < count; ++i) values_[i] = int();
        size_ = count;
        ++instances_;
    }

    virtual ~iarray() {
        boost::contract::old_ptr<int> old_instances;
        void BOOST_LOCAL_FUNCTION(const bind this_, bind& old_instances) {
            old_instances = BOOST_CONTRACT_OLDOF(this_->instances());
        } BOOST_LOCAL_FUNCTION_NAME(old)
        void BOOST_LOCAL_FUNCTION(const bind& old_instances) {
            BOOST_CONTRACT_ASSERT(iarray::instances() == *old_instances - 1);
        } BOOST_LOCAL_FUNCTION_NAME(post)
        boost::contract::check c = boost::contract::destructor(this)
                .old(old).postcondition(post);
    
        delete[] values_;
        --instances_;
    }

    virtual void push_back(int value, boost::contract::virtual_* v = 0) {
        boost::contract::old_ptr<unsigned> old_size;
        void BOOST_LOCAL_FUNCTION(const bind this_) {
            BOOST_CONTRACT_ASSERT(this_->size() < this_->capacity());
        } BOOST_LOCAL_FUNCTION_NAME(pre)
        void BOOST_LOCAL_FUNCTION(const bind v, const bind this_,
                bind& old_size) {
            old_size = BOOST_CONTRACT_OLDOF(v, this_->size());
        } BOOST_LOCAL_FUNCTION_NAME(old)
        void BOOST_LOCAL_FUNCTION(const bind this_, const bind& old_size) {
            BOOST_CONTRACT_ASSERT(this_->size() == *old_size + 1);
        } BOOST_LOCAL_FUNCTION_NAME(post)
        boost::contract::check c = boost::contract::public_function(v, this)
                .precondition(pre).old(old).postcondition(post);

        values_[size_++] = value;
    }
    
    unsigned capacity() const {
        // Check invariants.
        boost::contract::check c = boost::contract::public_function(this);
        return capacity_;
    }
    
    unsigned size() const {
        // Check invariants.
        boost::contract::check c = boost::contract::public_function(this);
        return size_;
    }

    static int instances() {
        // Check static invariants.
        boost::contract::check c = boost::contract::public_function<iarray>();
        return instances_;
    }

private:
    int* values_;
    unsigned capacity_;
    unsigned size_;
    static int instances_;
};

int iarray::instances_ = 0;

int main() {
    iarray a(3, 2);
    assert(a.capacity() == 3);
    assert(a.size() == 2);
    
    a.push_back('x');
    assert(a.size() == 3);
    
    return 0;
}

