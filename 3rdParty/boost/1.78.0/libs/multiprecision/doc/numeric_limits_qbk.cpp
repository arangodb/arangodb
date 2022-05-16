// Copyright Paul A. Bristow 2013

// Use, modification and distribution are subject to the
// Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// Program to list all numeric_limits items for any type to a file in Quickbook format.

// C standard http://www.open-std.org/jtc1/sc22/wg11/docs/n507.pdf
// SC22/WG11 N507 DRAFT INTERNATIONAL ISO/IEC STANDARD WD 10967-1
// Information technology Language independent arithmetic Part 1: Integer and Floating point arithmetic

/* E.3 C++
The programming language C++ is defined by ISO/IEC 14882:1998, Programming languages C++ [18].
An implementation should follow all the requirements of LIA-1 unless otherwise specified by
this language binding.
*/

// https://doi.org/10.1109/IEEESTD.1985.82928

// http://www.cesura17.net/~will/Professional/Research/Papers/retrospective.pdf

// http://www.exploringbinary.com/using-integers-to-check-a-floating-point-approximation/

// http://stackoverflow.com/questions/12466745/how-to-convert-float-to-doubleboth-stored-in-ieee-754-representation-without-loss


#ifdef _MSC_VER
#  pragma warning (disable : 4127)  // conditional expression is constant.
#  pragma warning (disable : 4100)  // unreferenced formal parameter.
#endif


#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <fstream>
#include <limits> // numeric_limits

#include <typeinfo>

#include <boost/version.hpp>
#include <boost/config.hpp>

// May need extra includes for other types, for example:
#include <boost/multiprecision/cpp_dec_float.hpp> // is decimal.
#include <boost/multiprecision/cpp_bin_float.hpp> // is binary.


// Assume that this will be run on MSVC to get the 32 or 64 bit info.
#ifdef _WIN32
  std::string bits32_64 = "32";
std::string filename = "numeric_limits_32_tables.qbk";
#else
  std::string bits32_64 = "64";
std::string filename = "numeric_limits_64_tables.qbk";
#endif

#ifdef INT32_T_MAX
  int i = INT256_T_MAX;
#endif

std::array<std::string, 16> integer_type_names =
{
"bool",
"char",
"unsigned char",
"char16_t",
"char32_t",
"short",
"unsigned short",
"int",
"unsigned int",
"long",
"unsigned long",
"long long",
"unsigned long long",
"int32_t",
//"uint32_t",
"int64_t",
//"uint64_t",
"int128_t",
//"uint128_t" // Too big?
//"int256_t",
//"uint256_t"
//"int512_t"
};

std::array<std::string, 6> float_type_names =
{
  "function", "float", "double", "long double", "cpp_dec_50", "cpp_bin_128"
};

// Table headings for integer constants.
std::array<std::string, 8> integer_constant_heads =
{
    "type", "signed", "bound", "modulo", "round", "radix", "digits", "digits10", // "max_digits10"
};

// Table headings for integer functions.
std::array<std::string, 2> integer_function_heads =
{
  "max", // "lowest",  assumes is same for all integer types, so not worth listing.
  "min"
};

// Table headings for float constants.
std::array<std::string, 12> float_constant_heads =
{
    "type", // "signed", "exact", "bound", // "modulo",
    "round", "radix", "digits", "digits10", "max_digits10", "min_exp", "min_exp10", "max_exp", "max_exp10", "tiny", "trap"
};

// Table headings for float functions.
std::array<std::string, 10> float_function_heads =
{
  "function", "max", "lowest", "min", "eps", "round", "infinity", "NaN", "sig_NaN", "denorm_min"
};

std::string versions()
{ // Build a string of info about Boost, platform, STL, etc.
  std::stringstream mess;
  //mess << "\n" << "Program:\n\" " __FILE__  << "\"\n"; // \n is mis-interpreted!
  mess << "\n" << "Program:\n numeric_limits_qbk.cpp \n";
#ifdef __TIMESTAMP__
  mess << __TIMESTAMP__;
#endif
  mess << "\nBuildInfo:\n" "  Platform " << BOOST_PLATFORM;
  mess << "\n  Compiler " BOOST_COMPILER;
#ifdef _MSC_FULL_VER
  mess << "\n  MSVC version "<< BOOST_STRINGIZE(_MSC_FULL_VER) << ".";
#endif
  mess << "\n  STL " BOOST_STDLIB;
  mess << "\n  Boost version " << BOOST_VERSION/100000 << "." << BOOST_VERSION/100 % 1000 << "." << BOOST_VERSION % 100 << std::endl;
  return mess.str();
} // std::string versions()

template <typename T>
void out_round_style(std::ostream& os)
{ //! Send short string describing STD::round_style to stream os.
    // os << "Round style is ";
    if (std::numeric_limits<T>::round_style == std::round_indeterminate)
    {
     os << "indeterminate" ;
    }
    else if (std::numeric_limits<T>::round_style == std::round_toward_zero)
    {
      os << "to zero" ;
    }
    else if (std::numeric_limits<T>::round_style == std::round_to_nearest)
    {
      os << "to nearest" ;
    }
    else if (std::numeric_limits<T>::round_style == std::round_toward_infinity)
    {
      os << "to infin[]"; // Or << "to \u221E"  << "to infinity" ;
    }
    else if (std::numeric_limits<T>::round_style == std::round_toward_neg_infinity)
    {
      os << "to -infin[]" ;
    }
    else
    {
      os << "undefined!";
      std::cout << "std::numeric_limits<T>::round_style is undefined value!" << std::endl;
    }
    return;
} // out_round_style(std::ostream& os);

template<typename T>
void integer_constants(std::string type_name, std::ostream& os)
{ //! Output a line of table integer constant values to ostream os.
    os << "\n["
          "[" << type_name << "]" ;
    os << "[" << (std::numeric_limits<T>::is_signed ? "signed" : "unsigned") << "]" ;
    // Is always exact for integer types, so removed:
    // os << "[" << (std::numeric_limits<T>::is_exact ? "exact" : "inexact") << "]" ;
    os << "[" << (std::numeric_limits<T>::is_bounded ? "bound" : "unbounded") << "]" ;
    os << "[" << (std::numeric_limits<T>::is_modulo ? "modulo" : "no") << "]" ;
    os << "[" ; out_round_style<T>(os); os << "]" ;
    os << "[" << std::numeric_limits<T>::radix << "]" ;
    os << "[" << std::numeric_limits<T>::digits << "]" ;
    os << "[" << std::numeric_limits<T>::digits10 << "]" ;
    // Undefined for integers, so removed:
   // os << "[" << std::numeric_limits<T>::max_digits10 << "]"
     os << "]";
} // void integer_constants


template<typename T>
void float_constants(std::string type_name, std::ostream& os)
{ //! Output a row of table values to `ostream` os.
    os << "\n["
          "[" << type_name << "]" ;
    //os << "[" << (std::numeric_limits<T>::is_signed ? "signed" : "unsigned") << "]" ;
    //os << "[" << (std::numeric_limits<T>::is_exact ? "exact" : "inexact") << "]" ;
    //os << "[" << (std::numeric_limits<T>::is_bounded ? "bound" : "no") << "]" ;
    // os << "[" << (std::numeric_limits<T>::is_modulo ? "modulo" : "no") << "]" ;
    os << "[" ; out_round_style<T>(os); os << "]" ;
    os << "[" << std::numeric_limits<T>::radix << "]" ;
    os << "[" << std::numeric_limits<T>::digits << "]" ;
    os << "[" << std::numeric_limits<T>::digits10 << "]" ;
    os << "[" << std::numeric_limits<T>::max_digits10 << "]";
    os << "[" << std::numeric_limits<T>::min_exponent << "]" ;
    os << "[" << std::numeric_limits<T>::min_exponent10 << "]" ;
    os << "[" << std::numeric_limits<T>::max_exponent << "]" ;
    os << "[" << std::numeric_limits<T>::max_exponent10  << "]" ;
    os << "[" << (std::numeric_limits<T>::tinyness_before ? "tiny" : "no") << "]" ;
    os << "[" << (std::numeric_limits<T>::traps ? "traps" : "no") << "]" ;
    os << "]" "\n"; // end of row.
} // void float_constants

/* Types across and two functions down.

template<typename T>
void integer_functions(std::string type_name, std::ostream& os)
{ //! Output a line of table integer function values to `ostream` os.
    os << "\n["
          "[" << type_name << "]" ;
    os << "[" << std::numeric_limits<T>::max() << "]" ;
    //os << "[" << std::numeric_limits<T>::lowest() << "]" ;  always == min for integer types,
    // so removed to save space.
    os << "[" << std::numeric_limits<T>::min() << "]"
      "]";
} // void integer_functions

*/

// Types down and two (or three) functions across.
template<typename T>
void integer_functions(std::string type_name, std::ostream& os)
{ //! Output a line of table integer function values to `ostream` os.
    os << "\n[" // start of row.
          "[" << type_name << "]" ;
    os << "[" << (std::numeric_limits<T>::max)() << "]" ;
   // os << "[" << std::numeric_limits<T>::lowest() << "]" ;
    os << "[" << (std::numeric_limits<T>::min)() << "]" ;
    os <<  "]"; // end of row.
} // void integer_functions


template<typename T>
void float_functions(std::string type_name, std::ostream& os)
{ //! Output a line of table float-point function values to `ostream` os.
    os << "\n[" // start of row.
          "[" << type_name << "]" ;
    os << "[" << (std::numeric_limits<T>::max)() << "]" ;
    os << "[" << (std::numeric_limits<T>::lowest)() << "]" ;
    os << "[" << (std::numeric_limits<T>::min)() << "]"
    os << "[" << std::numeric_limits<T>::epsilon() << "]"
    os << "[" << std::numeric_limits<T>::round_error() << "]"
    os << "[" << std::numeric_limits<T>::infinity() << "]"
    os << "[" << std::numeric_limits<T>::quiet_NaN() << "]"
    os << "[" << std::numeric_limits<T>::signaling_NaN() << "]"
    os << "[" << std::numeric_limits<T>::denorm_min() << "]"
      "]"; // end of row.
} // void float_functions

template<typename T>
int numeric_limits_list(std::string description)
{//!  Output numeric_limits for numeric_limits<T>, for example `numeric_limits_list<bool>()`.
  // This is not used for Quickbook format.
  // std::cout << versions()  << std::endl;

  std::cout << "\n" << description << "\n"  << std::endl; // int, int64_t rather than full long typeid(T).name().

  std::cout << "Type " << typeid(T).name() << " std::numeric_limits::<" << typeid(T).name() << ">\n"  << std::endl;
  // ull long typeid(T).name()

  if (std::numeric_limits<T>::is_specialized == false)
  {
    std::cout << "type " << typeid(T).name()  << " is not specialized for std::numeric_limits!" << std::endl;
    //return -1;
  }

  // Member constants.

  std::cout << (std::numeric_limits<T>::is_signed ? "is signed." : "unsigned.")  << std::endl;
  std::cout << (std::numeric_limits<T>::is_integer ? "is integer." : "not integer (fixed or float-point).")  << std::endl;
  std::cout << (std::numeric_limits<T>::is_exact ? "is exact." : "not exact.")  << std::endl;
  std::cout << (std::numeric_limits<T>::has_infinity ? "has infinity." : "no infinity.")  << std::endl;
  std::cout << (std::numeric_limits<T>::has_quiet_NaN ? "has quiet NaN." : "no quiet NaN.")  << std::endl;
  std::cout << (std::numeric_limits<T>::has_signaling_NaN ? "has signaling NaN." : "no signaling NaN.")  << std::endl;
  if (!std::numeric_limits<T>::is_integer)
  { // is floating_point
    std::cout << "Denorm style is " ;
    if (std::numeric_limits<T>::has_denorm == std::denorm_absent)
    {
      std::cout << "denorm_absent." << std::endl;
    }
    else if (std::numeric_limits<T>::has_denorm == std::denorm_present)
    {
      std::cout << "denorm_present." << std::endl;
    }
    else if (std::numeric_limits<T>::has_denorm == std::denorm_indeterminate)
    {
      std::cout << "denorm_indeterminate." << std::endl;
    }
    else
    {
      std::cout << "std::numeric_limits<T>::has_denorm unexpected value!" << std::endl;
    }

    std::cout << (std::numeric_limits<T>::has_denorm_loss ? "has denorm loss." : "no denorm loss.")  << std::endl;
    // true if a loss of accuracy is detected as a denormalization loss, rather than an inexact result.

    std::cout << "Round style is ";
    if (std::numeric_limits<T>::round_style == std::round_indeterminate)
    {
      std::cout << "round_indeterminate!" << std::endl;
    }
    else if (std::numeric_limits<T>::round_style == std::round_toward_zero)
    {
      std::cout << "round_toward_zero." << std::endl;
    }
    else if (std::numeric_limits<T>::round_style == std::round_to_nearest)
    {
      std::cout << "round_to_nearest." << std::endl;
    }
    else if (std::numeric_limits<T>::round_style == std::round_toward_infinity)
    {
      std::cout << "round_toward_infinity." << std::endl;
    }
    else if (std::numeric_limits<T>::round_style == std::round_toward_neg_infinity)
    {
      std::cout << "round_toward_neg_infinity." << std::endl;
    }
    else
    {
      std::cout << "undefined value!" << std::endl;
    }

  } // is floating_point

  std::cout << (std::numeric_limits<T>::is_iec559 ? "is IEC599." : "not IEC599.")  << std::endl;
  std::cout << (std::numeric_limits<T>::is_bounded ? "is bound." : "unbounded.")  << std::endl;
  std::cout << (std::numeric_limits<T>::is_modulo ? "is modulo." : "no modulo.")  << std::endl;
  std::cout << std::dec << "radix " << std::numeric_limits<T>::radix  << std::endl;
  std::cout << "digits " << std::numeric_limits<T>::digits  << std::endl;
  std::cout << "digits10 " << std::numeric_limits<T>::digits10  << std::endl;

  std::cout.precision(std::numeric_limits<T>::max_digits10); // Full precision for floating-point values like max, min ...

  std::cout << "max_digits10 " << std::numeric_limits<T>::max_digits10  << std::endl;
  std::cout << "min_exponent " << std::numeric_limits<T>::min_exponent  << std::endl;
  std::cout << "min_exponent10 " << std::numeric_limits<T>::min_exponent10  << std::endl;
  std::cout << "max_exponent " << std::numeric_limits<T>::max_exponent  << std::endl;
  std::cout << "max_exponent10 " << std::numeric_limits<T>::max_exponent10  << std::endl;

  std::cout << (std::numeric_limits<T>::tinyness_before ? "Can tiny values before rounding." : "no tinyness_before.")  << std::endl;
  // true if the type can detect tiny values before rounding; false if it cannot.
  std::cout << (std::numeric_limits<T>::traps ? "traps" : "no trapping.")  << std::endl;
  // Whether trapping that reports on arithmetic exceptions is implemented for a type.

  std::cout << "Member functions." << std::endl;
  // (assumes operator<< for type T is available).
  // If floating-point then hex format may not be available.

  std::cout << "max = " << (std::numeric_limits<T>::max)() << std::endl;
  //if (std::numeric_limits<T>::is_integer)
  //{
  //  std::cout << "    = " << std::hex << std::numeric_limits<T>::max() << std::endl;
  //}

  std::cout << "lowest = " << std::dec << std::numeric_limits<T>::lowest() << std::endl;
  //if (std::numeric_limits<T>::is_integer)
  //{
  //   std::cout << "       = " << std::hex << std::numeric_limits<T>::lowest() << std::endl;
  //}

  std::cout << "min = " << (std::dec << std::numeric_limits<T>::min)() << std::endl;
  //if (std::numeric_limits<T>::is_integer)
  //{
  //  std::cout << "    = " << std::hex << std::numeric_limits<T>::min() << std::endl;
  //}

  std::cout << "epsilon = " << std::dec << std::numeric_limits<T>::epsilon() << std::endl;
  //if (std::numeric_limits<T>::is_integer)
  //{
  //  std::cout << "        = " << std::hex << std::numeric_limits<T>::epsilon() << std::endl;
  //}
  // round_error is always zero for integer types.
  // round_error is usually 1/2 = (T)0.5 for floating-point types.
  // round_error is ? for fixed-point.
  std::cout << "round_error = " << std::numeric_limits<T>::round_error() << " ULP." << std::endl;

  std::cout << "infinity = " << std::dec << std::numeric_limits<T>::infinity() << std::endl;
  std::cout << "         = " << std::hex << std::numeric_limits<T>::infinity() << std::endl;

  std::cout << "quiet_NaN = " << std::dec << std::numeric_limits<T>::quiet_NaN() << std::endl;
  std::cout << "          = " << std::hex << std::numeric_limits<T>::quiet_NaN() << std::endl;

  std::cout << "signaling_NaN = " << std::dec << std::numeric_limits<T>::signaling_NaN() << std::endl;
  std::cout << "              = " << std::hex << std::numeric_limits<T>::signaling_NaN() << std::endl;

  //  Only meaningful if (std::numeric_limits<T>::has_denorm == std::denorm_present)
  // so might not bother to show if absent?
  std::cout << "denorm_min = " << std::numeric_limits<T>::denorm_min()  << std::endl;
  std::cout << "           = " << std::numeric_limits<T>::denorm_min()  << std::endl;
  return 0;
}

int main()
{



  try
  {
    using namespace boost::multiprecision;

    std::cout << versions() << std::endl;

    std::ofstream fout(filename, std::ios_base::out);
    if (!fout.is_open())
    {
      std::cout << "Unable to open file " << filename << " for output.\n" << std::endl;
      return -1; // boost::EXIT_FAILURE;
    }
    fout <<
      "[/""\n"
      "Copyright 2013 Paul A. Bristow.""\n"
      "Copyright 2013 John Maddock.""\n"
      "Distributed under the Boost Software License, Version 1.0.""\n"
      "(See accompanying file LICENSE_1_0.txt or copy at""\n"
      "http://www.boost.org/LICENSE_1_0.txt).""\n"
      "]""\n"
    << std::endl;

    fout << "[section:limits"<< bits32_64 << " Numeric limits for " << bits32_64 << "-bit platform]" << std::endl;

    // Output platform version info (32 or 64).
    fout << "These tables were generated using the following program and options:\n\n"
      "[pre""\n"
      << versions()
      << "]""\n"
      << std::endl;

    fout << "[table:integral_constants Integer types constants (`std::numeric_limits<T>::is_integer == true` && is_exact == true)" "\n"
      "[";

    for (size_t i = 0; i < integer_constant_heads.size(); i++)
    { // signed, bound, modulo ...
      fout << "[" << integer_constant_heads[i] << "]" ;
    }
    fout << "]";

    integer_constants<bool>("bool", fout);
    integer_constants<char>("char", fout);
    integer_constants<unsigned char>("unsigned char", fout);
    integer_constants<char16_t>("char16_t", fout);
    integer_constants<char32_t>("char32_t", fout);
    integer_constants<short>("short", fout);
    integer_constants<unsigned short>("unsigned short", fout);
    integer_constants<int>("int", fout);
    integer_constants<unsigned int>("unsigned", fout);
    integer_constants<long>("long", fout);
    integer_constants<unsigned long>("unsigned long", fout);
    integer_constants<long long>("long long", fout);
    integer_constants<unsigned long long>("unsigned long long", fout);
    integer_constants<int32_t>("int32_t", fout);
    integer_constants<uint32_t>("uint32_t", fout);
    integer_constants<int64_t>("int64_t", fout);
    integer_constants<uint64_t>("uint64_t", fout);
    integer_constants<int128_t>("int128_t", fout);
    integer_constants<uint128_t>("uint128_t", fout);
    integer_constants<int256_t>("int256_t", fout);
    integer_constants<uint256_t>("uint256_t", fout);
   // integer_constants<int512_t>("int512_t", fout);
    //integer_constants<uint512_t>("uint512_t", fout); // Too big?
    integer_constants<cpp_int>("cpp_int", fout);

    fout << "\n]\n\n";


    fout << "[table:integral_functions Integer types functions (`std::numeric_limits<T>::is_integer == true && std::numeric_limits<T>::min() == std::numeric_limits<T>::lowest()` )" "\n"
      "[";
    // Display types across the page, and 2 (or 3) functions as rows.

    fout << "[function]";
    for (size_t i = 0; i < integer_function_heads.size(); i++)
    {
      fout << "[" << integer_function_heads[i] << "]" ;
    }
    fout << "]"; // end of headings.
    integer_functions<bool>("bool", fout);
    //integer_functions<char>("char", fout); // Need int value not char.
    fout << "\n[" // start of row.
       "[" << "char"<< "]" ;
    fout << "[" << static_cast<int>(std::numeric_limits<char>::max)() << "]" ;
   // fout << "[" << (std::numeric_limits<T>::lowest)() << "]" ;
    fout << "[" << static_cast<int>(std::numeric_limits<char>::min)() << "]" ;
    fout <<  "]"; // end of row.
    //integer_functions<unsigned char>("unsigned char", fout); // Need int value not char.
    fout << "\n[" // start of row.
       "[" << "unsigned char"<< "]" ;
    fout << "[" << static_cast<int>(std::numeric_limits<unsigned char>::max)() << "]" ;
   // fout << "[" << std::numeric_limits<unsigned char>::lowest() << "]" ;
    fout << "[" << static_cast<int>(std::numeric_limits<unsigned char>::min)() << "]" ;
    fout <<  "]"; // end of row.

    integer_functions<char16_t>("char16_t", fout);
    integer_functions<char32_t>("char32_t", fout);
    integer_functions<short>("short", fout);
    integer_functions<unsigned short>("unsigned short", fout);
    integer_functions<int>("int", fout);
    integer_functions<unsigned int>("unsigned int", fout);
    integer_functions<long>("long", fout);
    integer_functions<unsigned long>("unsigned long", fout);
    integer_functions<long long>("long long", fout);
    integer_functions<unsigned long long>("unsigned long long", fout);
    integer_functions<int32_t>("int32_t", fout);
    integer_functions<int64_t>("int64_t", fout);
    integer_functions<int128_t>("int128_t", fout);
    fout << "]" "\n";  // end of table;


    //fout << "[[max]"
    //  << "[" << std::numeric_limits<bool>::max() << "]"
    //  << "[" << static_cast<int>(std::numeric_limits<char>::max()) << "]"
    //  << "[" << static_cast<int>(std::numeric_limits<unsigned char>::max()) << "]"
    //  << "[" << static_cast<int>(std::numeric_limits<char16_t>::max()) << "]"
    //  << "[" << static_cast<int>(std::numeric_limits<char32_t>::max()) << "]"
    //  << "[" << std::numeric_limits<short>::max() << "]"
    //  << "[" << std::numeric_limits<unsigned short>::max() << "]"
    //  << "[" << std::numeric_limits<int>::max() << "]"
    //  << "[" << std::numeric_limits<unsigned int>::max() << "]"
    //  << "[" << std::numeric_limits<long>::max() << "]"
    //  << "[" << std::numeric_limits<unsigned long>::max() << "]"
    //  << "[" << std::numeric_limits<long long>::max() << "]"
    //  << "[" << std::numeric_limits<unsigned long long>::max() << "]"
    //  << "[" << std::numeric_limits<int32_t>::max() << "]"
    //  << "[" << std::numeric_limits<int64_t>::max() << "]"
    //  << "[" << std::numeric_limits<int128_t>::max() << "]"
    //  //<< "[" << std::numeric_limits<int256_t>::max() << "]"  // too big?
    //  //<< "[" << std::numeric_limits<int512_t>::max() << "]" // too big?
    //  << "]" "\n";
    ///*  Assume lowest() is not useful as == min for all integer types.
    // */

    //fout << "[[min]"
    //  << "[" << std::numeric_limits<bool>::min() << "]"
    //  << "[" << static_cast<int>(std::numeric_limits<char>::min()) << "]"
    //  << "[" << static_cast<int>(std::numeric_limits<unsigned char>::min()) << "]"
    //  << "[" << static_cast<int>(std::numeric_limits<char16_t>::min()) << "]"
    //  << "[" << static_cast<int>(std::numeric_limits<char32_t>::min()) << "]"
    //  << "[" << std::numeric_limits<short>::min() << "]"
    //  << "[" << std::numeric_limits<unsigned short>::min() << "]"
    //  << "[" << std::numeric_limits<int>::min() << "]"
    //  << "[" << std::numeric_limits<unsigned int>::min() << "]"
    //  << "[" << std::numeric_limits<long>::min() << "]"
    //  << "[" << std::numeric_limits<unsigned long>::min() << "]"
    //  << "[" << std::numeric_limits<long long>::min() << "]"
    //  << "[" << std::numeric_limits<unsigned long long>::min() << "]"
    //  << "[" << std::numeric_limits<int32_t>::min() << "]"
    //  << "[" << std::numeric_limits<int64_t>::min() << "]"
    //  << "[" << std::numeric_limits<int128_t>::min() << "]"
    //  // << "[" << std::numeric_limits<int256_t>::min() << "]"  // too big?
    //  // << "[" << std::numeric_limits<int512_t>::min() << "]"  // too big?
    //  << "]""\n";



  // Floating-point

    typedef number<cpp_dec_float<50> > cpp_dec_float_50; // 50 decimal digits.
    typedef number<cpp_bin_float<113> > bin_128bit_double_type; // == Binary rare long double.

    fout <<
      //"[table:float_functions Floating-point types constants (`std::numeric_limits<T>::is_integer == false && std::numeric_limits<T>::is_modulo == false` )" "\n"
      "[table:float_functions Floating-point types constants (`std::numeric_limits<T>::is_integer==false && is_signed==true && is_modulo==false && is_exact==false && is_bound==true`)" "\n"
      "[";
    for (size_t i = 0; i < float_constant_heads.size(); i++)
    {
      fout << "[" << float_constant_heads[i] << "]" ;
    }
    fout << "]"; // end of headings.

    float_constants<float>("float", fout);
    float_constants<double>("double", fout);
    float_constants<long double>("long double", fout);
    float_constants<cpp_dec_float_50>("cpp_dec_float_50", fout);
    float_constants<bin_128bit_double_type>("bin_128bit_double_type", fout);
    fout << "]" "\n";  // end of table;

    fout <<
      "[table:float_functions Floating-point types functions (`std::numeric_limits<T>::is_integer == false`)" "\n"
      "[";

    for (size_t i = 0; i < float_type_names.size(); i++)
    {
      fout << "[" << float_type_names[i] << "]" ;
    }
    fout << "]"; // end of headings.

    fout << "[[max]"
      << "[" << (std::numeric_limits<float>::max)() << "]"
      << "[" << (std::numeric_limits<double>::max)() << "]"
//#if LDBL_MANT_DIG > DBL_MANT_DIG
    // Perhaps to test Long double is not just a duplication of double (but need change is headings too).
      << "[" << (std::numeric_limits<long double>::max)() << "]"
//#endif
      << "[" << (std::numeric_limits<cpp_dec_float_50>::max)() << "]"
      << "[" << (std::numeric_limits<bin_128bit_double_type >::max)() << "]"
      << "]" "\n"; // end of row.

    fout << "[[min]"
      << "[" << (std::numeric_limits<float>::min)() << "]"
      << "[" << (std::numeric_limits<double>::min)() << "]"
//#if LDBL_MANT_DIG > DBL_MANT_DIG
    // Long double is not just a duplication of double.
      << "[" << (std::numeric_limits<long double>::min)() << "]"
//#endif
      << "[" << (std::numeric_limits<cpp_dec_float_50 >::min)() << "]"
      << "[" << (std::numeric_limits<bin_128bit_double_type >::min)() << "]"
      << "]" "\n"; // end of row.

    fout << "[[epsilon]"
      << "[" << std::numeric_limits<float>::epsilon() << "]"
      << "[" << std::numeric_limits<double>::epsilon() << "]"
//#if LDBL_MANT_DIG > DBL_MANT_DIG
    // Long double is not just a duplication of double.
      << "[" << std::numeric_limits<long double>::epsilon() << "]"
//#endif
      << "[" << std::numeric_limits<cpp_dec_float_50 >::epsilon() << "]"
      << "[" << std::numeric_limits<bin_128bit_double_type >::epsilon() << "]"
      << "]" "\n"; // end of row.

    fout << "[[round_error]"
      << "[" << std::numeric_limits<float>::round_error() << "]"
      << "[" << std::numeric_limits<double>::round_error() << "]"
//#if LDBL_MANT_DIG > DBL_MANT_DIG
    // Long double is not just a duplication of double.
      << "[" << std::numeric_limits<long double>::round_error() << "]"
//#endif
      << "[" << std::numeric_limits<cpp_dec_float_50 >::round_error() << "]"
      << "[" << std::numeric_limits<bin_128bit_double_type >::round_error() << "]"
      << "]" "\n"; // end of row.

    fout << "[[infinity]"
      << "[" << std::numeric_limits<float>::infinity() << "]"
      << "[" << std::numeric_limits<double>::infinity() << "]"
//#if LDBL_MANT_DIG > DBL_MANT_DIG
    // Long double is not just a duplication of double.
      << "[" << std::numeric_limits<long double>::infinity() << "]"
//#endif
      << "[" << std::numeric_limits<cpp_dec_float_50 >::infinity() << "]"
      << "[" << std::numeric_limits<bin_128bit_double_type >::infinity() << "]"
      << "]" "\n"; // end of row.

    fout << "[[quiet_NaN]"
      << "[" << std::numeric_limits<float>::quiet_NaN() << "]"
      << "[" << std::numeric_limits<double>::quiet_NaN() << "]"
//#if LDBL_MANT_DIG > DBL_MANT_DIG
    // Long double is not just a duplication of double.
      << "[" << std::numeric_limits<long double>::quiet_NaN() << "]"
//#endif
      << "[" << std::numeric_limits<cpp_dec_float_50 >::quiet_NaN() << "]"
      << "[" << std::numeric_limits<bin_128bit_double_type >::quiet_NaN() << "]"
      << "]" "\n"; // end of row.

    fout << "[[signaling_NaN]"
      << "[" << std::numeric_limits<float>::signaling_NaN() << "]"
      << "[" << std::numeric_limits<double>::signaling_NaN() << "]"
//#if LDBL_MANT_DIG > DBL_MANT_DIG
    // Long double is not just a duplication of double.
      << "[" << std::numeric_limits<long double>::signaling_NaN() << "]"
//#endif
      << "[" << std::numeric_limits<cpp_dec_float_50 >::signaling_NaN() << "]"
      << "[" << std::numeric_limits<bin_128bit_double_type >::signaling_NaN() << "]"
      << "]" "\n"; // end of row.

    fout << "[[denorm_min]"
      << "[" << std::numeric_limits<float>::denorm_min() << "]"
      << "[" << std::numeric_limits<double>::denorm_min() << "]"
//#if LDBL_MANT_DIG > DBL_MANT_DIG
    // Long double is not just a duplication of double.
      << "[" << std::numeric_limits<long double>::denorm_min() << "]"
//#endif
      << "[" << std::numeric_limits<cpp_dec_float_50 >::denorm_min() << "]"
      << "[" << std::numeric_limits<bin_128bit_double_type >::denorm_min() << "]"
      << "]" "\n"; // end of row.



     fout << "]" "\n";  // end of table;


       fout <<  "\n\n"
    "[endsect] [/section:limits32  Numeric limits for 32-bit platform]" "\n" << std::endl;



    fout.close();
  }
  catch(std::exception ex)
  {
    std::cout << "exception thrown: " << ex.what() << std::endl;
  }


} // int main()

/*
  Description: Autorun "J:\Cpp\Misc\Debug\numeric_limits_qbk.exe"

  Program: I:\boost-sandbox\multiprecision.cpp_bin_float\libs\multiprecision\doc\numeric_limits_qbk.cpp
  Wed Aug 28 14:17:21 2013
  BuildInfo:
    Platform Win32
    Compiler Microsoft Visual C++ version 10.0
    MSVC version 160040219.
    STL Dinkumware standard library version 520
    Boost version 1.55.0

  */

