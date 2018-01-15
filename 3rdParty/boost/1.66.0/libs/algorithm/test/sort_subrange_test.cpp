#include <boost/config.hpp>
#include <boost/algorithm/sort_subrange.hpp>
#include <boost/algorithm/cxx11/is_sorted.hpp>

#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

#include <vector>
#include <iostream>

#if __cplusplus >= 201103L
#include <random>

std::default_random_engine gen;
template<typename RandomIt>
void do_shuffle(RandomIt first, RandomIt last)
{ std::shuffle(first, last, gen); }
#else
template<typename RandomIt>
void do_shuffle(RandomIt first, RandomIt last)
{ std::random_shuffle(first, last); }
#endif

namespace ba = boost::algorithm;

template <typename Iter>
void check_sequence ( Iter first, Iter last, Iter sf, Iter sl )
{
	if (sf == sl) return;
	for (Iter i = first; i < sf; ++i)
		BOOST_CHECK(*i < *sf);
	BOOST_CHECK(ba::is_sorted(sf, sl));
	for (Iter i = sl; i < last; ++i)
		BOOST_CHECK(*(sl-1) < *i);
}

template <typename Iter, typename Pred>
void check_sequence ( Iter first, Iter last, Iter sf, Iter sl, Pred p )
{
	if (sf == sl) return;
	for (Iter i = first; i < sf; ++i)
		BOOST_CHECK(p(*i, *sf));
	BOOST_CHECK(ba::is_sorted(sf, sl, p));
	for (Iter i = sl; i < last; ++i)
		BOOST_CHECK(p(*(sl-1), *i));

}

// 	for ( int i = 0; i < v.size(); ++i )
// 		std::cout << v[i] << ' ';
// 	std::cout << std::endl;


BOOST_AUTO_TEST_CASE( test_main )
{
	{
	std::vector<int> v;
	for ( int i = 0; i < 10; ++i )
		v.push_back(i);
	
	const std::vector<int>::iterator b = v.begin();
	ba::sort_subrange(b, v.end(), b + 3, b + 6);
	check_sequence   (b, v.end(), b + 3, b + 6);

	BOOST_CHECK_EQUAL(v[3], 3);
	BOOST_CHECK_EQUAL(v[4], 4);
	BOOST_CHECK_EQUAL(v[5], 5);
	
//	Mix them up and try again - single element
	do_shuffle(v.begin(), v.end());
	ba::sort_subrange(b, v.end(), b + 7, b + 8);
	check_sequence   (b, v.end(), b + 7, b + 8);

	BOOST_CHECK_EQUAL(v[7], 7);

//	Mix them up and try again - at the end
	do_shuffle(v.begin(), v.end());
	ba::sort_subrange(b, v.end(), b + 7, v.end());
	check_sequence   (b, v.end(), b + 7, v.end());

	BOOST_CHECK_EQUAL(v[7], 7);
	BOOST_CHECK_EQUAL(v[8], 8);
	BOOST_CHECK_EQUAL(v[9], 9);

//	Mix them up and try again - at the beginning
	do_shuffle(v.begin(), v.end());
	ba::sort_subrange(b, v.end(), b, b + 2);
	check_sequence   (b, v.end(), b, b + 2);

	BOOST_CHECK_EQUAL(v[0], 0);
	BOOST_CHECK_EQUAL(v[1], 1);

//	Mix them up and try again - empty subrange
	do_shuffle(v.begin(), v.end());
	ba::sort_subrange(b, v.end(), b, b);
	check_sequence   (b, v.end(), b, b);

//	Mix them up and try again - entire subrange
	do_shuffle(v.begin(), v.end());
	ba::sort_subrange(b, v.end(), b, v.end());
	check_sequence   (b, v.end(), b, v.end());
	}

	{
	std::vector<int> v;
	for ( int i = 0; i < 10; ++i )
		v.push_back(i);
	
	const std::vector<int>::iterator b = v.begin();
	ba::sort_subrange(b, v.end(), b + 3, b + 6, std::greater<int>());
	check_sequence   (b, v.end(), b + 3, b + 6, std::greater<int>());

	BOOST_CHECK_EQUAL(v[3], 6);
	BOOST_CHECK_EQUAL(v[4], 5);
	BOOST_CHECK_EQUAL(v[5], 4);

//	Mix them up and try again - single element
	do_shuffle(v.begin(), v.end());
	ba::sort_subrange(b, v.end(), b + 7, b + 8, std::greater<int>());
	check_sequence   (b, v.end(), b + 7, b + 8, std::greater<int>());

	BOOST_CHECK_EQUAL(v[7], 2);

//	Mix them up and try again - at the end
	do_shuffle(v.begin(), v.end());
	ba::sort_subrange(b, v.end(), b + 7, v.end(), std::greater<int>());
	check_sequence   (b, v.end(), b + 7, v.end(), std::greater<int>());

	BOOST_CHECK_EQUAL(v[7], 2);
	BOOST_CHECK_EQUAL(v[8], 1);
	BOOST_CHECK_EQUAL(v[9], 0);

//	Mix them up and try again - at the beginning
	do_shuffle(v.begin(), v.end());
	ba::sort_subrange(b, v.end(), b, b + 2, std::greater<int>());
	check_sequence   (b, v.end(), b, b + 2, std::greater<int>());

	BOOST_CHECK_EQUAL(v[0], 9);
	BOOST_CHECK_EQUAL(v[1], 8);

//	Mix them up and try again - empty subrange
	do_shuffle(v.begin(), v.end());
	ba::sort_subrange(b, v.end(), b, b, std::greater<int>());
	check_sequence   (b, v.end(), b, b, std::greater<int>());

//	Mix them up and try again - entire subrange
	do_shuffle(v.begin(), v.end());
	ba::sort_subrange(b, v.end(), b, v.end(), std::greater<int>());
	check_sequence   (b, v.end(), b, v.end(), std::greater<int>());
	}
}
