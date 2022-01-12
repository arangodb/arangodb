/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library
    http://www.boost.org/

    Copyright (c) 2001-2010 Hartmut Kaiser.
    Copyright (c) 2020 Jeff Trull. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#if !defined(BOOST_WAVE_CHECK_MACRO_NAMING_INCLUDED)
#define BOOST_WAVE_CHECK_MACRO_NAMING_INCLUDED

#include <boost/wave/token_ids.hpp>
#include <boost/wave/preprocessing_hooks.hpp>
#include <boost/regex.hpp>

#include <string>
#include <set>

///////////////////////////////////////////////////////////////////////////////
//
//  The macroname_preprocessing_hooks policy class is used to record the
//  use of macros within a header file.
//
//  This policy type is used as a template parameter to the
//  boost::wave::context<> object.
//
///////////////////////////////////////////////////////////////////////////////
class macroname_preprocessing_hooks
:   public boost::wave::context_policies::default_preprocessing_hooks
{
public:
    macroname_preprocessing_hooks(boost::regex const & macro_regex,
                                  std::set<std::string>& bad_macros,
                                  std::string& include_guard)
        : macro_regex_(macro_regex),
          bad_macros_(bad_macros),
          include_guard_(include_guard),
          suppress_includes_(false)
    {}

    ///////////////////////////////////////////////////////////////////////////
    //
    //  Monitor macro definitions to verify they follow the required convention
    //  by overriding the defined_macro hook
    //
    ///////////////////////////////////////////////////////////////////////////

    template <typename ContextT, typename TokenT,
              typename ParametersT, typename DefinitionT>
    void defined_macro(ContextT const & /* ctx */, TokenT const &name,
                       bool /* is_functionlike */, ParametersT const & /* parameters */,
                       DefinitionT const & /* definition */, bool is_predefined)
    {
        using namespace boost::wave;
        if (!is_predefined &&
            !regex_match(name.get_value().c_str(), macro_regex_))
            bad_macros_.insert(name.get_value().c_str());
    }

    // Wave only reports include guards in files that were actually included
    // as a result we have to mock up the inclusion process. This means
    // constructing a fake "includer" file in memory, and only permitting one
    // level of includes (as we only want to analyze the header itself)

    ///////////////////////////////////////////////////////////////////////////
    //
    //  Suppress includes of files other than the one we are analyzing
    //  using found_include_directive
    //
    ///////////////////////////////////////////////////////////////////////////

    template <typename ContextT>
    bool found_include_directive(ContextT const& /* ctx */,
                                 std::string const & filename,
                                 bool /* include_next */)
    {
        return suppress_includes_;
    }

    ///////////////////////////////////////////////////////////////////////////
    //
    //  Suppress includes beyond the first level by setting our flag
    //  from opened_include_file
    //
    ///////////////////////////////////////////////////////////////////////////

    template <typename ContextT>
    void opened_include_file(ContextT const& /* ctx */,
                             std::string const & /* rel_filename */,
                             std::string const & /* abs_filename */,
                             bool /* is_system_include */)
    {
        suppress_includes_ = true;
    }

    // we only study one file, so no need to restore the ability to include

    ///////////////////////////////////////////////////////////////////////////
    //
    //  Record detected include guard macros
    //
    ///////////////////////////////////////////////////////////////////////////

    template <typename ContextT>
    void detected_include_guard(ContextT const& /* ctx */,
                                std::string filename,
                                std::string const& include_guard)
    {
        include_guard_ = include_guard;
    }


private:
    boost::regex const & macro_regex_;
    std::set<std::string>& bad_macros_;
    std::string& include_guard_;
    bool suppress_includes_;
};

#endif // !defined(BOOST_WAVE_CHECK_MACRO_NAMING_INCLUDED)
