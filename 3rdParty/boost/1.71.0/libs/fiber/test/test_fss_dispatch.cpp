// Copyright (C) 2001-2003
// William E. Kempf
// Copyright (C) 2007 Anthony Williams
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <mutex>

#include <boost/test/unit_test.hpp>

#include <boost/fiber/all.hpp>

boost::fibers::mutex check_mutex;
boost::fibers::mutex fss_mutex;
int fss_instances = 0;
int fss_total = 0;

struct fss_value_t {
    fss_value_t() {
        std::unique_lock<boost::fibers::mutex> lock(fss_mutex);
        ++fss_instances;
        ++fss_total;
        value = 0;
    }
    ~fss_value_t() {
        std::unique_lock<boost::fibers::mutex> lock(fss_mutex);
        --fss_instances;
    }
    int value;
};

boost::fibers::fiber_specific_ptr<fss_value_t> fss_value;

void fss_fiber() {
    fss_value.reset(new fss_value_t());
    for (int i=0; i<1000; ++i) {
        int& n = fss_value->value;
        if (n != i) {
            std::unique_lock<boost::fibers::mutex> lock(check_mutex);
            BOOST_CHECK_EQUAL(n, i);
        }
        ++n;
    }
}

void fss() {
    fss_instances = 0;
    fss_total = 0;

    boost::fibers::fiber f1( boost::fibers::launch::dispatch, fss_fiber);
    boost::fibers::fiber f2( boost::fibers::launch::dispatch, fss_fiber);
    boost::fibers::fiber f3( boost::fibers::launch::dispatch, fss_fiber);
    boost::fibers::fiber f4( boost::fibers::launch::dispatch, fss_fiber);
    boost::fibers::fiber f5( boost::fibers::launch::dispatch, fss_fiber);
    f1.join();
    f2.join();
    f3.join();
    f4.join();
    f5.join();

    std::cout
        << "fss_instances = " << fss_instances
        << "; fss_total = " << fss_total
        << "\n";
    std::cout.flush();

    BOOST_CHECK_EQUAL(fss_instances, 0);
    BOOST_CHECK_EQUAL(fss_total, 5);
}

void test_fss() {
    boost::fibers::fiber( boost::fibers::launch::dispatch, fss).join();
}

bool fss_cleanup_called=false;

struct Dummy {
};

void fss_custom_cleanup(Dummy* d) {
    delete d;
    fss_cleanup_called=true;
}

boost::fibers::fiber_specific_ptr<Dummy> fss_with_cleanup(fss_custom_cleanup);

void fss_fiber_with_custom_cleanup() {
    fss_with_cleanup.reset(new Dummy);
}

void fss_with_custom_cleanup() {
    boost::fibers::fiber f( boost::fibers::launch::dispatch, fss_fiber_with_custom_cleanup);
    try {
        f.join();
    } catch(...) {
        f.join();
        throw;
    }

    BOOST_CHECK(fss_cleanup_called);
}

void test_fss_with_custom_cleanup() {
    boost::fibers::fiber( boost::fibers::launch::dispatch, fss_with_custom_cleanup).join();
}

Dummy* fss_object=new Dummy;

void fss_fiber_with_custom_cleanup_and_release() {
    fss_with_cleanup.reset(fss_object);
    fss_with_cleanup.release();
}

void do_test_fss_does_no_cleanup_after_release() {
    fss_cleanup_called=false;
    boost::fibers::fiber f( boost::fibers::launch::dispatch, fss_fiber_with_custom_cleanup_and_release);
    try {
        f.join();
    } catch(...) {
        f.join();
        throw;
    }

    BOOST_CHECK(!fss_cleanup_called);
    if(!fss_cleanup_called) {
        delete fss_object;
    }
}

struct dummy_class_tracks_deletions {
    static unsigned deletions;

    ~dummy_class_tracks_deletions() {
        ++deletions;
    }
};

unsigned dummy_class_tracks_deletions::deletions=0;

boost::fibers::fiber_specific_ptr<dummy_class_tracks_deletions> fss_with_null_cleanup(NULL);

void fss_fiber_with_null_cleanup(dummy_class_tracks_deletions* delete_tracker) {
    fss_with_null_cleanup.reset(delete_tracker);
}

void do_test_fss_does_no_cleanup_with_null_cleanup_function() {
    dummy_class_tracks_deletions* delete_tracker=new dummy_class_tracks_deletions;
    boost::fibers::fiber f( boost::fibers::launch::dispatch, [&delete_tracker](){
        fss_fiber_with_null_cleanup( delete_tracker); });
    try {
        f.join();
    } catch(...) {
        f.join();
        throw;
    }

    BOOST_CHECK(!dummy_class_tracks_deletions::deletions);
    if(!dummy_class_tracks_deletions::deletions) {
        delete delete_tracker;
    }
}

void test_fss_does_no_cleanup_after_release() {
    boost::fibers::fiber( boost::fibers::launch::dispatch, do_test_fss_does_no_cleanup_after_release).join();
}

void test_fss_does_no_cleanup_with_null_cleanup_function() {
    boost::fibers::fiber( boost::fibers::launch::dispatch, do_test_fss_does_no_cleanup_with_null_cleanup_function).join();
}


void fiber_with_local_fss_ptr() {
    {
        boost::fibers::fiber_specific_ptr<Dummy> local_fss(fss_custom_cleanup);

        local_fss.reset(new Dummy);
    }
    BOOST_CHECK(fss_cleanup_called);
    fss_cleanup_called=false;
}

void fss_does_not_call_cleanup_after_ptr_destroyed() {
    boost::fibers::fiber( boost::fibers::launch::dispatch, fiber_with_local_fss_ptr).join();
    BOOST_CHECK(!fss_cleanup_called);
}

void test_fss_does_not_call_cleanup_after_ptr_destroyed() {
    boost::fibers::fiber( boost::fibers::launch::dispatch, fss_does_not_call_cleanup_after_ptr_destroyed).join();
}


void fss_cleanup_not_called_for_null_pointer() {
    boost::fibers::fiber_specific_ptr<Dummy> local_fss(fss_custom_cleanup);
    local_fss.reset(new Dummy);
    fss_cleanup_called=false;
    local_fss.reset(0);
    BOOST_CHECK(fss_cleanup_called);
    fss_cleanup_called=false;
    local_fss.reset(new Dummy);
    BOOST_CHECK(!fss_cleanup_called);
}

void test_fss_cleanup_not_called_for_null_pointer() {
    boost::fibers::fiber( boost::fibers::launch::dispatch, fss_cleanup_not_called_for_null_pointer).join();
}


void fss_at_the_same_adress() {
  for(int i=0; i<2; i++) {
    boost::fibers::fiber_specific_ptr<Dummy> local_fss(fss_custom_cleanup);
    local_fss.reset(new Dummy);
    fss_cleanup_called=false;
    BOOST_CHECK(fss_cleanup_called);
    fss_cleanup_called=false;
    BOOST_CHECK(!fss_cleanup_called);
  }
}

void test_fss_at_the_same_adress() {
    boost::fibers::fiber( boost::fibers::launch::dispatch, fss_at_the_same_adress).join();
}

boost::unit_test::test_suite* init_unit_test_suite(int, char*[]) {
    boost::unit_test::test_suite* test =
        BOOST_TEST_SUITE("Boost.Fiber: fss test suite");

    test->add(BOOST_TEST_CASE(test_fss));
    test->add(BOOST_TEST_CASE(test_fss_with_custom_cleanup));
    test->add(BOOST_TEST_CASE(test_fss_does_no_cleanup_after_release));
    test->add(BOOST_TEST_CASE(test_fss_does_no_cleanup_with_null_cleanup_function));
    test->add(BOOST_TEST_CASE(test_fss_does_not_call_cleanup_after_ptr_destroyed));
    test->add(BOOST_TEST_CASE(test_fss_cleanup_not_called_for_null_pointer));

    return test;
}
