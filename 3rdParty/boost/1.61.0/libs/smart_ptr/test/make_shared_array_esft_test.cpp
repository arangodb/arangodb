/*
(c) 2012-2015 Glen Joseph Fernandes
<glenjofe -at- gmail.com>

Distributed under the Boost Software
License, Version 1.0.
http://boost.org/LICENSE_1_0.txt
*/
#include <boost/core/lightweight_test.hpp>
#include <boost/smart_ptr/enable_shared_from_this.hpp>
#include <boost/smart_ptr/make_shared.hpp>

class type
    : public boost::enable_shared_from_this<type> {
public:
    static unsigned int instances;
    explicit type() {
        instances++;
    }
    ~type() {
        instances--;
    }
private:
    type(const type&);
    type& operator=(const type&);
};

unsigned int type::instances = 0;

int main()
{
    BOOST_TEST(type::instances == 0);
    {
        boost::shared_ptr<type[]> a1 = boost::make_shared<type[]>(3);
        try {
            a1[0].shared_from_this();
            BOOST_ERROR("shared_from_this did not throw");
        } catch (...) {
            BOOST_TEST(type::instances == 3);
        }
    }
    BOOST_TEST(type::instances == 0);
    {
        boost::shared_ptr<type[]> a1 = boost::make_shared_noinit<type[]>(3);
        try {
            a1[0].shared_from_this();
            BOOST_ERROR("shared_from_this did not throw");
        } catch (...) {
            BOOST_TEST(type::instances == 3);
        }
    }
    return boost::report_errors();
}
