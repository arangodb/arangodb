/*-----------------------------------------------------------------------------+    
Copyright (c) 2008-2009: Joachim Faulhaber
+------------------------------------------------------------------------------+
   Distributed under the Boost Software License, Version 1.0.
      (See accompanying file LICENCE.txt or copy at
           http://www.boost.org/LICENSE_1_0.txt)
+-----------------------------------------------------------------------------*/
#define BOOST_TEST_MODULE icl::casual unit test

#define BOOST_ICL_TEST_CHRONO

#include <libs/icl/test/disable_test_warnings.hpp>

#include <string>
#include <vector>
#include <boost/mpl/list.hpp>
#include "../unit_test_unwarned.hpp"


// interval instance types
#include "../test_type_lists.hpp"
#include "../test_value_maker.hpp"

#include <boost/rational.hpp>

#include <boost/type_traits/is_same.hpp>

#include <boost/icl/gregorian.hpp>
#include <boost/icl/ptime.hpp>

#include <boost/icl/interval_map.hpp>
#include <boost/icl/interval_set.hpp>
#include <boost/icl/interval.hpp>



namespace my
{

class Spy
{
public:
    Spy():_val(0){
        std::cout << "Spy() ";
    }
    Spy(int val):_val(val){}
    int val()const { return _val; }

    Spy& operator += (const Spy& rhs){
        std::cout << "+= ";
        return *this; 
    }
    Spy& operator -= (const Spy& rhs){ if(_val == rhs.val()) _val=0; return *this; }
    Spy& operator &= (const Spy& rhs){ if(_val != rhs.val()) _val=0; return *this; }

private:
    int _val;
};

bool operator == (const Spy& lhs, const Spy& rhs){ return lhs.val() == rhs.val(); }
bool operator <  (const Spy& lhs, const Spy& rhs){ return lhs.val()  < rhs.val(); }

template<class CharType, class CharTraits>
std::basic_ostream<CharType, CharTraits> &operator<<
  (std::basic_ostream<CharType, CharTraits> &stream, Spy const& value)
{
    return stream << value.val();
}

} // namespace my 




using namespace std;
using namespace boost;
using namespace unit_test;
using namespace boost::icl;

BOOST_AUTO_TEST_CASE(casual)
{
    using namespace my;


    typedef interval_map<int, Spy> SpyMapT;
    SpyMapT imap;

    //imap += make_pair(interval<int>::right_open( 0, 8),  Spy(1));

    imap.add(imap.begin(), make_pair(interval<int>::right_open( 0, 8),  Spy(1)));

    BOOST_CHECK_EQUAL(true, true);
}


