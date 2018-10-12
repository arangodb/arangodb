#include <iostream>
#include <iomanip>
#include <string.h>
#include <boost/preprocessor/stringize.hpp>

static unsigned int indent = 4;
static unsigned int width = 40;

using std::cout;
using std::istream;

void print_separator()
{
   std::cout <<
"\n\n*********************************************************************\n\n";
}

void print_macro(const char* name, const char* value)
{
   // if name == value+1 then then macro is not defined,
   // in which case we don't print anything:
   if(0 != strcmp(name, value+1))
   {
      for(unsigned i = 0; i < indent; ++i) std::cout.put(' ');
      std::cout << std::setw(width);
      cout.setf(istream::left, istream::adjustfield);
      std::cout << name;
      if(value[1])
      {
         // macro has a value:
         std::cout << value << "\n";
      }
      else
      {
         // macro is defined but has no value:
         std::cout << " [no value]\n";
      }
   }
}

#define PRINT_MACRO(X) print_macro(#X, BOOST_PP_STRINGIZE(=X))

void print_macros()
{
  
  print_separator();
  
  PRINT_MACRO(__GCCXML__);
  PRINT_MACRO(__WAVE__);
  PRINT_MACRO(__MWERKS__);
  PRINT_MACRO(__EDG__);
  PRINT_MACRO(_MSC_VER);
  PRINT_MACRO(__clang__);
  PRINT_MACRO(__DMC__);
  PRINT_MACRO(__BORLANDC__);
  PRINT_MACRO(__IBMC__);
  PRINT_MACRO(__IBMCPP__);
  PRINT_MACRO(__SUNPRO_CC);
  PRINT_MACRO(__CUDACC__);
  PRINT_MACRO(__PATHSCALE__);
  PRINT_MACRO(__CODEGEARC__);
  PRINT_MACRO(__HP_aCC);
  PRINT_MACRO(__SC__);
  PRINT_MACRO(__MRC__);
  PRINT_MACRO(__PGI);
  PRINT_MACRO(__INTEL_COMPILER);
  PRINT_MACRO(__GNUC__);
  PRINT_MACRO(__GXX_EXPERIMENTAL_CXX0X__);
  
  print_separator();
  
  PRINT_MACRO(__cplusplus);
  PRINT_MACRO(__STDC_VERSION__);
  PRINT_MACRO(__EDG_VERSION__);
  PRINT_MACRO(__INTELLISENSE__);
  PRINT_MACRO(__WAVE_HAS_VARIADICS__);
  
  print_separator();
  
  PRINT_MACRO(BOOST_PP_CONFIG_ERRORS);
  PRINT_MACRO(BOOST_PP_CONFIG_EXTENDED_LINE_INFO);
  PRINT_MACRO(BOOST_PP_CONFIG_FLAGS());
  PRINT_MACRO(BOOST_PP_VARIADICS);
  PRINT_MACRO(BOOST_PP_VARIADICS_MSVC);
}

int main()
{

  print_macros();

  return 0;
}
