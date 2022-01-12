// Copyright (c) 2018-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// Example from https://github.com/apolukhin/magic_get/issues/21


// boost::pfr::for_each_field crashes when sizeof(MyConfig) > 248 (probably >= 256)

#include <boost/pfr.hpp>

#include <iostream>
#include <type_traits>

template <typename T>
class CfgAttrib {
public:
    using value_type = T;

    const char* getAttrName() const { return name; }
    const T& getValue() const { return value; }

    static constexpr std::true_type is_config_field{};

    const char* const name;
    T value;
    //char dummy[8];
};


// a marker class for the code reflection
struct CfgSection {
    const char* const name{ "UNNAMED" };
    static constexpr std::false_type is_config_field{};
};

// a marker class for the code reflection
struct CfgSubSection {
    const char* const name{ "UNNAMED" };
    static constexpr std::false_type is_config_field{};
};


// all configuration data apart from audio and midi devices, which is handled by special juce support
// the class is supposed to be iterated with boost::pfr library.
// Thus its members must met the requirements (aggregate initializeable)
class MyConfig {
public:
    // Configuration / Section Data fields

    CfgSection                  sectionMain{ "section1" };
    CfgAttrib<unsigned>         attr1{ "attr1", 1 };

    CfgSection                  section2{ "section2" };
    CfgAttrib<unsigned>         attr3{ "attr3", 13 };
    CfgAttrib<unsigned>         attr4{ "attr4", 2};
    CfgAttrib<unsigned>         attr5{ "attr5", 0 };
    CfgAttrib<unsigned>         attr6{ "attr6", 6 };

    CfgSection                  section3{ "section3" };
    CfgAttrib<long long int>    attr7{ "attr7", 0 };

    CfgSection                  section4{ "section4" };
    CfgAttrib<long long int>    attr8{ "attr8", 0 };
    CfgAttrib<long long int>    attr9{ "attr9", 0 };
    CfgAttrib<long long int>    attr10{ "attr10", 0 };

    CfgSection                  section5{ "section5" };
    CfgAttrib<long long int>    attr11{ "attr11", 0 };

    CfgSection                  section666{ "section666" };
    CfgAttrib<long long int>    attr12{ "attr12", 0 };
    CfgAttrib<unsigned>         attr13{ "attr13", 0 };
};



template <class T>
void printer(const T& value, std::true_type) {
    std::cout << "- " << value.getAttrName() << ": " << value.getValue() << std::ends;
}

template <class T>
void printer(const T& value, std::false_type) {
    std::cout << "Section \"" << value.name << "\":" << std::ends;
}


int main() {
    std::cout << "sizeof(MyConfig) = " << sizeof(MyConfig) << std::ends;

    MyConfig aCfg;
    boost::pfr::for_each_field(aCfg, [](auto& value) {
        printer(value, value.is_config_field);
    });

#if BOOST_PFR_USE_CPP17
    boost::pfr::get<0>(aCfg); // also C1202
#endif
}
