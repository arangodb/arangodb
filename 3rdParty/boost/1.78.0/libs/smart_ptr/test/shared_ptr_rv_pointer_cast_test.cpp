//
//  shared_ptr_rv_pointer_cast_test.cpp
//
//  Copyright (c) 2016 Chris Glover
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/shared_ptr.hpp>
#include <boost/core/lightweight_test.hpp>

#if !defined( BOOST_NO_CXX11_RVALUE_REFERENCES )

struct X
{};

struct Y: public X
{};

struct U
{
    virtual ~U() {}
};

struct V: public U
{};

struct W : public U
{};

int main()
{
    {
        boost::shared_ptr<X> px(new Y);

        boost::shared_ptr<Y> py1 = boost::static_pointer_cast<Y>(px);
        boost::shared_ptr<Y> py2 = boost::static_pointer_cast<Y>(std::move(px));
        BOOST_TEST(!px);
        BOOST_TEST(px.use_count() == 0);
        BOOST_TEST(py1.get() == py2.get());
        BOOST_TEST(!(py1 < py2 || py2 < py1));
        BOOST_TEST(py1.use_count() == 2);
        BOOST_TEST(py2.use_count() == 2);
    }

    {
        boost::shared_ptr<int const volatile> px(new int);

        boost::shared_ptr<int> px2 = boost::const_pointer_cast<int>(px);
        boost::shared_ptr<int> px3 = boost::const_pointer_cast<int>(std::move(px));
        BOOST_TEST(!px);
        BOOST_TEST(px.use_count() == 0);
        BOOST_TEST(px2.get() == px3.get());
        BOOST_TEST(!(px2 < px3 || px2 < px3));
        BOOST_TEST(px2.use_count() == 2);
        BOOST_TEST(px3.use_count() == 2);
    }

    {
        boost::shared_ptr<char> pv(reinterpret_cast<char*>(new Y));

        boost::shared_ptr<Y> py1 = boost::reinterpret_pointer_cast<Y>(pv);
        boost::shared_ptr<Y> py2 = boost::reinterpret_pointer_cast<Y>(std::move(pv));
        BOOST_TEST(!pv);
        BOOST_TEST(pv.use_count() == 0);
        BOOST_TEST(py1.get() == py2.get());
        BOOST_TEST(!(py1 < py2 || py2 < py1));
        BOOST_TEST(py1.use_count() == 2);
        BOOST_TEST(py2.use_count() == 2);
    }

#if !defined( BOOST_NO_RTTI )
    {
        boost::shared_ptr<U> pu(new V);

        boost::shared_ptr<V> pv1 = boost::dynamic_pointer_cast<V>(pu);
        boost::shared_ptr<V> pv2 = boost::dynamic_pointer_cast<V>(std::move(pu));
        BOOST_TEST(!pu);
        BOOST_TEST(pu.use_count() == 0);
        BOOST_TEST(pv1.get() == pv2.get());
        BOOST_TEST(!(pv1 < pv2 || pv2 < pv1));
        BOOST_TEST(pv1.use_count() == 2);
        BOOST_TEST(pv2.use_count() == 2);
    }

    {
        boost::shared_ptr<U> pu(new V);
        boost::shared_ptr<W> pw = boost::dynamic_pointer_cast<W>(std::move(pu));
        BOOST_TEST(!pw);
        BOOST_TEST(pu);
    }
#endif // !defined( BOOST_NO_RTTI )

    return boost::report_errors();
}

#else // !defined( BOOST_NO_CXX11_RVALUE_REFERENCES )

int main()
{
    return 0;
}

#endif // !defined( BOOST_NO_CXX11_RVALUE_REFERENCES )
