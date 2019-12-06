// Copyright (C) 2015 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_THREAD_VERSION 4
#include <boost/thread.hpp>
#include <boost/thread/tss.hpp>
#include <boost/shared_ptr.hpp>
#include <iostream>

struct A
{
    void DoWork()
    {
        std::cout << "A: doing work\n";
        if (!m_ptr.get())
            m_ptr.reset(new WorkSpace());
        // do not very much
        for (size_t i = 0; i < 10; ++i)
            m_ptr->a += 10;
    }

private:
    struct WorkSpace
    {
        int a;
        WorkSpace() : a(0) {}
    };
    boost::thread_specific_ptr<WorkSpace> m_ptr;
};

struct B
{
    void DoWork()
    {
        std::cout << "B: doing work\n";
        if (!m_ptr.get())
            m_ptr.reset(new A());
        m_ptr->DoWork();
    }
private:
    boost::thread_specific_ptr<A> m_ptr;
};

struct C
{
    void DoWork()
    {
        std::cout << "C: doing work\n";
        if (!m_ptr.get())
            m_ptr.reset(new B());
        m_ptr->DoWork();
    }
private:
    boost::thread_specific_ptr<B> m_ptr;
};

int main(int ac, char** av)
{
    std::cout << "test starting\n";
    boost::shared_ptr<C> p_C(new C);
    boost::thread cWorker(&C::DoWork, p_C);
    cWorker.join();
    std::cout << "test stopping\n";
    return 0;
}



