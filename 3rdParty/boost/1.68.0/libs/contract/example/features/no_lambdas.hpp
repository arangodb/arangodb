
#ifndef NO_LAMBDAS_HPP_
#define NO_LAMBDAS_HPP_

// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <boost/contract.hpp>

//[no_lambdas_hpp
class iarray :
        private boost::contract::constructor_precondition<iarray> {
public:
    static void static_invariant() {
        BOOST_CONTRACT_ASSERT(instances() >= 0);
    }

    void invariant() const {
        BOOST_CONTRACT_ASSERT(size() <= capacity());
    }

    explicit iarray(unsigned max, unsigned count = 0);
        static void constructor_precondition(unsigned const max,
                unsigned const count) {
            BOOST_CONTRACT_ASSERT(count <= max);
        }
        static void constructor_old(boost::contract::old_ptr<int>&
                old_instances) {
            old_instances = BOOST_CONTRACT_OLDOF(instances());
        }
        void constructor_postcondition(unsigned const max, unsigned const count,
                boost::contract::old_ptr<int> const old_instances) const {
            BOOST_CONTRACT_ASSERT(capacity() == max);
            BOOST_CONTRACT_ASSERT(size() == count);
            BOOST_CONTRACT_ASSERT(instances() == *old_instances + 1);
        }

    virtual ~iarray();
        void destructor_old(boost::contract::old_ptr<int>& old_instances)
                const {
            old_instances = BOOST_CONTRACT_OLDOF(instances());
        }
        static void destructor_postcondition(boost::contract::old_ptr<int> const
                old_instances) {
            BOOST_CONTRACT_ASSERT(instances() == *old_instances - 1);
        }

    virtual void push_back(int value, boost::contract::virtual_* v = 0);
        void push_back_precondition() const {
            BOOST_CONTRACT_ASSERT(size() < capacity());
        }
        void push_back_old(boost::contract::virtual_* v,
                boost::contract::old_ptr<unsigned>& old_size) const {
            old_size = BOOST_CONTRACT_OLDOF(v, size());
        }
        void push_back_postcondition(
                boost::contract::old_ptr<unsigned> const old_size) const {
            BOOST_CONTRACT_ASSERT(size() == *old_size + 1);
        }
    
    unsigned capacity() const;
    unsigned size() const;

    static int instances();

private:
    int* values_;
    unsigned capacity_;
    unsigned size_;
    static int instances_;
};
//]

#endif // #include guard

