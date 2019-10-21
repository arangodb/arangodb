/* 
   Copyright (c) Marshall Clow 2010-2012.

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    For more information, see http://www.boost.org
*/

#include <boost/algorithm/searching/boyer_moore.hpp>
#include <boost/algorithm/searching/boyer_moore_horspool.hpp>
#include <boost/algorithm/searching/knuth_morris_pratt.hpp>

#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <string>
#include <vector>


namespace ba = boost::algorithm;

template <typename Iter>
std::string make_str ( Iter first, std::size_t len ) {
    std::string retVal ( len + 2, '\'' );
    std::copy ( first, first+len, retVal.begin () + 1);
    return retVal;
    }

namespace {

//  Check using iterators
    template<typename Container>
    void check_one_iter ( const Container &haystack, const std::string &needle, int expected ) {
        typedef typename Container::const_iterator iter_type;
        typedef typename std::pair<iter_type, iter_type> ret_type;
        typedef std::string::const_iterator pattern_type;

        iter_type hBeg = haystack.begin ();
        iter_type hEnd = haystack.end ();
        pattern_type nBeg = needle.begin ();
        pattern_type nEnd = needle.end ();

//      iter_type ret0  = std::search                     (hBeg, hEnd, nBeg, nEnd);
        ret_type ret1  = ba::boyer_moore_search          (hBeg, hEnd, nBeg, nEnd);
        ret_type ret1r = ba::boyer_moore_search          (haystack, nBeg, nEnd);
        ret_type ret2  = ba::boyer_moore_horspool_search (hBeg, hEnd, nBeg, nEnd);
        ret_type ret3  = ba::knuth_morris_pratt_search   (hBeg, hEnd, nBeg, nEnd);

        iter_type it0  = std::search                     (hBeg, hEnd, nBeg, nEnd);
//         iter_type it1  = ret1.first;
//         iter_type it1r = ret1r.first;
//         iter_type it2  = ret2.first;
//         iter_type it3  = ret3.first;
        const int dist = ret1.first == hEnd ? -1 : std::distance ( hBeg, ret1.first );

        std::cout << "(Iterators) Pattern is " << needle.length () << ", haysstack is " << haystack.length () << " chars long; " << std::endl;
        try {
            if ( it0 != ret1.first ) {
                throw std::runtime_error ( 
                    std::string ( "results mismatch between std::search and boyer-moore search" ));
                }

            if ( ret1.first != ret1r.first || ret1.second != ret1r.second ) {
                throw std::runtime_error ( 
                    std::string ( "results mismatch between iterator and range boyer_moore search" ));
                }

            if ( ret1.first != ret2.first || ret1.second != ret2.second ) {
                throw std::runtime_error ( 
                    std::string ( "results mismatch between boyer-moore and boyer-moore-horspool search" ));
                }

            if ( ret1.first != ret3.first || ret1.second != ret3.second ) {
                throw std::runtime_error ( 
                    std::string ( "results mismatch between boyer-moore and knuth-morris-pratt search" ));
                }

            }

        catch ( ... ) {
            std::cout << "Searching for: " << needle << std::endl;
            std::cout << "Expected: " << expected << "\n";
            std::cout << "  std:    " << std::distance ( hBeg, it0 ) << "\n";
            std::cout << "  bm:     " << std::distance ( hBeg, ret1.first ) << "\n";
            std::cout << "  bm(r):  " << std::distance ( hBeg, ret1r.first ) << "\n";
            std::cout << "  bmh:    " << std::distance ( hBeg, ret2.first ) << "\n";
            std::cout << "  kpm:    " << std::distance ( hBeg, ret3.first )<< "\n";
            std::cout << std::flush;
            throw ;
            }

        BOOST_CHECK_EQUAL ( dist, expected );
        }

//  Check using pointers
//    We're assuming that the container implements contiguous storage here.
    template<typename Container>
    void check_one_pointer ( const Container &haystack, const std::string &needle, int expected ) {
        typedef const typename Container::value_type *ptr_type;
        typedef typename std::pair<ptr_type, ptr_type> ret_type;

        ptr_type hBeg = haystack.size () == 0 ? NULL : &*haystack.begin ();
        ptr_type hEnd = hBeg + haystack.size ();
        ptr_type nBeg = needle.size () == 0 ? NULL : &*needle.begin ();
        ptr_type nEnd = nBeg + needle.size ();

        ptr_type it0  = std::search                     (hBeg, hEnd, nBeg, nEnd);
        ret_type ret1 = ba::boyer_moore_search          (hBeg, hEnd, nBeg, nEnd);
        ret_type ret2 = ba::boyer_moore_horspool_search (hBeg, hEnd, nBeg, nEnd);
        ret_type ret3 = ba::knuth_morris_pratt_search   (hBeg, hEnd, nBeg, nEnd);
        const int dist = ret1.first == hEnd ? -1 : std::distance ( hBeg, ret1.first );

        std::cout << "(Pointers) Pattern is " << needle.length () << ", haysstack is " << haystack.length () << " chars long; " << std::endl;
        try {
            if ( it0 != ret1.first ) {
                throw std::runtime_error ( 
                    std::string ( "results mismatch between std::search and boyer-moore search" ));
                }

            if ( ret1.first != ret2.first || ret1.second != ret2.second ) {
                throw std::runtime_error ( 
                    std::string ( "results mismatch between boyer-moore and boyer-moore-horspool search" ));
                }

            if ( ret1.first != ret3.first || ret1.second != ret3.second ) {
                throw std::runtime_error ( 
                    std::string ( "results mismatch between boyer-moore and knuth-morris-pratt search" ));
                }

            }

        catch ( ... ) {
            std::cout << "Searching for: " << needle << std::endl;
            std::cout << "Expected: " << expected << "\n";
            std::cout << "  std:    " << std::distance ( hBeg, it0 ) << "\n";
            std::cout << "  bm:     " << std::distance ( hBeg, ret1.first ) << "\n";
            std::cout << "  bmh:    " << std::distance ( hBeg, ret2.first ) << "\n";
            std::cout << "  kpm:    " << std::distance ( hBeg, ret3.first )<< "\n";
            std::cout << std::flush;
            throw ;
            }

        BOOST_CHECK_EQUAL ( dist, expected );
        }

//  Check using objects
    template<typename Container>
    void check_one_object ( const Container &haystack, const std::string &needle, int expected ) {
        typedef typename Container::const_iterator iter_type;
        typedef typename std::pair<iter_type, iter_type> ret_type;
        typedef std::string::const_iterator pattern_type;

        iter_type hBeg = haystack.begin ();
        iter_type hEnd = haystack.end ();
        pattern_type nBeg = needle.begin ();
        pattern_type nEnd = needle.end ();

        ba::boyer_moore<pattern_type>          bm_r  = ba::make_boyer_moore ( needle );
        ba::boyer_moore<pattern_type>          bm    ( nBeg, nEnd );
        ba::boyer_moore_horspool<pattern_type> bmh   ( nBeg, nEnd );
        ba::knuth_morris_pratt<pattern_type>   kmp   ( nBeg, nEnd );
        
        iter_type it0   = std::search  (hBeg, hEnd, nBeg, nEnd);
        ret_type ret1   = bm           (hBeg, hEnd);
        ret_type ret1r  = bm           (haystack);
        ret_type retr1  = bm_r         (hBeg, hEnd);
       ret_type retr1r = bm_r         (haystack);
        ret_type ret2   = bmh          (hBeg, hEnd);
        ret_type ret3   = kmp          (hBeg, hEnd);
        const int dist  = ret1.first == hEnd ? -1 : std::distance ( hBeg, ret1.first );

        std::cout << "(Objects) Pattern is " << needle.length () << ", haysstack is " << haystack.length () << " chars long; " << std::endl;
        try {
            if ( it0 != ret1.first ) {
                throw std::runtime_error ( 
                    std::string ( "results mismatch between std::search and boyer-moore search" ));
                }

            if ( ret1.first != ret1r.first || ret1.second != ret1r.second ) {
                throw std::runtime_error ( 
                    std::string ( "results mismatch between iterator and range boyer_moore search(1)" ));
                }

            if ( ret1.first != retr1.first || ret1.second != retr1.second ) {
                throw std::runtime_error ( 
                    std::string ( "results mismatch between iterator and range boyer_moore search(2)" ));
                }

            if ( ret1.first != retr1r.first || ret1.second != retr1r.second ) {
                throw std::runtime_error ( 
                    std::string ( "results mismatch between iterator and range boyer_moore search(3)" ));
                }

            if ( ret1.first != ret2.first || ret1.second != ret2.second ) {
                throw std::runtime_error ( 
                    std::string ( "results mismatch between boyer-moore and boyer-moore-horspool search" ));
                }

            if ( ret1.first != ret3.first || ret1.second != ret3.second ) {
                throw std::runtime_error ( 
                    std::string ( "results mismatch between boyer-moore and knuth-morris-pratt search" ));
                }

            }

        catch ( ... ) {
            std::cout << "Searching for: " << needle << std::endl;
            std::cout << "Expected:  " << expected << "\n";
            std::cout << "  std:     " << std::distance ( hBeg, it0 ) << "\n";
            std::cout << "  bm:      " << std::distance ( hBeg, ret1.first ) << "\n";
            std::cout << "  bm(r1):  " << std::distance ( hBeg, ret1r.first ) << "\n";
            std::cout << "  bm(r2):  " << std::distance ( hBeg, retr1.first ) << "\n";
            std::cout << "  bm(r3):  " << std::distance ( hBeg, retr1r.first ) << "\n";
            std::cout << "  bmh:     " << std::distance ( hBeg, ret2.first ) << "\n";
            std::cout << "  kpm:    " << std::distance ( hBeg, ret3.first )<< "\n";
            std::cout << std::flush;
            throw ;
            }

        BOOST_CHECK_EQUAL ( dist, expected );
        }


    template<typename Container>
    void check_one ( const Container &haystack, const std::string &needle, int expected ) {
        check_one_iter ( haystack, needle, expected );
        check_one_pointer ( haystack, needle, expected );
        check_one_object ( haystack, needle, expected );
        }
    }


BOOST_AUTO_TEST_CASE( test_main )
{
    std::string haystack1 ( "NOW AN FOWE\220ER ANNMAN THE ANPANMANEND" );
    std::string needle1   ( "ANPANMAN" );
    std::string needle2   ( "MAN THE" );
    std::string needle3   ( "WE\220ER" );
    std::string needle4   ( "NOW " );   //  At the beginning
    std::string needle5   ( "NEND" );   //  At the end
    std::string needle6   ( "NOT FOUND" );  // Nowhere
    std::string needle7   ( "NOT FO\340ND" );   // Nowhere

    std::string haystack2 ( "ABC ABCDAB ABCDABCDABDE" );
    std::string needle11  ( "ABCDABD" );

    std::string haystack3 ( "abra abracad abracadabra" );
    std::string needle12  ( "abracadabra" );

    std::string needle13   ( "" );
    std::string haystack4  ( "" );

    check_one ( haystack1, needle1, 26 );
    check_one ( haystack1, needle2, 18 );
    check_one ( haystack1, needle3,  9 );
    check_one ( haystack1, needle4,  0 );
    check_one ( haystack1, needle5, 33 );
    check_one ( haystack1, needle6, -1 );
    check_one ( haystack1, needle7, -1 );

    check_one ( needle1, haystack1, -1 );   // cant find long pattern in short corpus
    check_one ( haystack1, haystack1, 0 );  // find something in itself
    check_one ( haystack2, haystack2, 0 );  // find something in itself

    check_one ( haystack2, needle11, 15 );
    check_one ( haystack3, needle12, 13 );

    check_one ( haystack1, needle13, 0 );   // find the empty string 
    check_one ( haystack4, needle1, -1 );  // can't find in an empty haystack

//  Mikhail Levin <svarneticist@gmail.com> found a problem, and this was the test
//  that triggered it.

  const std::string mikhail_pattern =   
"GATACACCTACCTTCACCAGTTACTCTATGCACTAGGTGCGCCAGGCCCATGCACAAGGGCTTGAGTGGATGGGAAGGA"
"TGTGCCCTAGTGATGGCAGCATAAGCTACGCAGAGAAGTTCCAGGGCAGAGTCACCATGACCAGGGACACATCCACGAG"
"CACAGCCTACATGGAGCTGAGCAGCCTGAGATCTGAAGACACGGCCATGTATTACTGTGGGAGAGATGTCTGGAGTGGT"
"TATTATTGCCCCGGTAATATTACTACTACTACTACTACATGGACGTCTGGGGCAAAGGGACCACG"
;
    const std::string mikhail_corpus = std::string (8, 'a') + mikhail_pattern;

    check_one ( mikhail_corpus, mikhail_pattern, 8 );
    }
