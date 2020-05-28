//
// Copyright 2015-2019 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/type_index/ctti_type_index.hpp>

#include <algorithm>
#include <string>

#include <boost/core/lightweight_test.hpp>

const char* hello1 = "Hello word";
const char* hello1_end = hello1 + sizeof("Hello word");
const char* hello2 = "Hello word, pal!";
const char* hello2_end = hello2 + sizeof("Hello word, pal!");

void strcmp_same() {
    using boost::typeindex::detail::constexpr_strcmp;

    BOOST_TEST(
        constexpr_strcmp(hello1, hello1) == 0
    );

    BOOST_TEST(
        constexpr_strcmp(hello2, hello2) == 0
    );

    BOOST_TEST(
        constexpr_strcmp(hello1, hello2) != 0
    );

    BOOST_TEST(
        constexpr_strcmp(hello2, hello1) != 0
    );

    BOOST_TEST(
        (constexpr_strcmp(hello2, hello1) < 0)
        ==
        (std::strcmp(hello2, hello1) < 0)
    );

    BOOST_TEST(
        (constexpr_strcmp(hello1, hello2) < 0)
        ==
        (std::strcmp(hello1, hello2) < 0)
    );
}

void search_same() {
    using boost::typeindex::detail::constexpr_search;
    BOOST_TEST(
        constexpr_search(hello1, hello1_end, hello2, hello2_end) == std::search(hello1, hello1_end, hello2, hello2_end)
    );

    BOOST_TEST(
        constexpr_search(hello2, hello2_end, hello1, hello1_end) == std::search(hello2, hello2_end, hello1, hello1_end)
    );

    const char* word = "word";
    const char* word_end = word + sizeof("word") - 1;
    BOOST_TEST(
        constexpr_search(hello1, hello1_end, word, word_end) == std::search(hello1, hello1_end, word, word_end)
    );

    BOOST_TEST(
        constexpr_search(hello2, hello2_end, word, word_end) == std::search(hello2, hello2_end, word, word_end)
    );
}

template <class T, std::size_t N>
BOOST_CXX14_CONSTEXPR bool in_namespace(const char (&ns)[N]) BOOST_NOEXCEPT {
    BOOST_CXX14_CONSTEXPR const char* name = boost::typeindex::ctti_type_index::type_id<T>().raw_name();
    for (std::size_t i = 0; i < N - 1; ++i)
        if (name[i] != ns[i])
            return false;
            
    return true;
}

template <class T>
BOOST_CXX14_CONSTEXPR bool is_boost_namespace() BOOST_NOEXCEPT {
    return in_namespace<T>("boost::") || in_namespace<T>("class boost::") || in_namespace<T>("struct boost::");
}

void constexpr_test() {
    using namespace boost::typeindex;

    BOOST_CXX14_CONSTEXPR ctti_type_index t_int0 = ctti_type_index::type_id<int>();
    (void)t_int0;

    BOOST_CXX14_CONSTEXPR ctti_type_index t_short0 = ctti_type_index::type_id<short>();
    (void)t_short0;

    BOOST_CXX14_CONSTEXPR ctti_type_index t_int1 = ctti_type_index::type_id<int>();
    (void)t_int1;

    BOOST_CXX14_CONSTEXPR ctti_type_index t_short1 = ctti_type_index::type_id<short>();
    (void)t_short1;

// Following tests are known to fail on _MSC_VER == 1916.
#if !defined(_MSC_VER) || _MSC_VER > 1916

    BOOST_CXX14_CONSTEXPR bool same0 = (t_int0 == t_int1);
    BOOST_TEST(same0);

    BOOST_CXX14_CONSTEXPR bool same1 = (t_short1 == t_short0);
    BOOST_TEST(same1);

    BOOST_CXX14_CONSTEXPR bool same2 = (t_int1 == t_int1);
    BOOST_TEST(same2);

    BOOST_CXX14_CONSTEXPR bool same3 = (t_short0 == t_short0);
    BOOST_TEST(same3);

    BOOST_CXX14_CONSTEXPR bool same4 = !(t_short0 < t_short0 || t_short0 > t_short0);
    BOOST_TEST(same4);

    BOOST_CXX14_CONSTEXPR bool same5 = (t_short0 <= t_short0 && t_short0 >= t_short0);
    BOOST_TEST(same5);


    BOOST_CXX14_CONSTEXPR bool not_same0 = (t_int0 != t_short1);
    BOOST_TEST(not_same0);

    BOOST_CXX14_CONSTEXPR bool not_same1 = (t_int1 != t_short0);
    BOOST_TEST(not_same1);

    BOOST_CXX14_CONSTEXPR bool not_same2 = (t_int1 < t_short0 || t_int1 > t_short0);
    BOOST_TEST(not_same2);


    BOOST_CXX14_CONSTEXPR const char* int_name = t_int0.name();
    BOOST_TEST(*int_name != '\0');

    BOOST_CXX14_CONSTEXPR const char* short_name = t_short0.name();
    BOOST_TEST(*short_name != '\0');

    BOOST_CXX14_CONSTEXPR bool in_namespace = is_boost_namespace<ctti_type_index>();
    BOOST_TEST(in_namespace);

    BOOST_CXX14_CONSTEXPR bool not_in_namespace = !is_boost_namespace<std::string>();
    BOOST_TEST(not_in_namespace);

#endif // #if !defined(_MSC_VER) || _MSC_VER > 1916
}


int main() {
    strcmp_same();
    search_same();
    constexpr_test();
    return boost::report_errors();
}

