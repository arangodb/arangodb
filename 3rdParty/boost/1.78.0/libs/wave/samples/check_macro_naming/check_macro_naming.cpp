/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library
    Sample demonstrating the usage of advanced preprocessor hooks.

    http://www.boost.org/

    Copyright (c) 2001-2010 Hartmut Kaiser.
    Copyright (c) 2020 Jeff Trull. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include "check_macro_naming.hpp"
#include "libs/filesystem/include/boost/filesystem/file_status.hpp"

///////////////////////////////////////////////////////////////////////////////
//  Utilities from the rest of Boost
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include <boost/bind/bind.hpp>

///////////////////////////////////////////////////////////////////////////////
//  Wave itself
#include <boost/wave.hpp>

///////////////////////////////////////////////////////////////////////////////
// Include the lexer stuff
#include <boost/wave/cpplexer/cpp_lex_token.hpp>    // token class
#include <boost/wave/cpplexer/cpp_lex_iterator.hpp> // lexer class

#include <iostream>
#include <string>
#include <set>
#include <algorithm>

void process_header(std::string const & filename,
                    boost::regex const & re) {
    using namespace boost::wave;

    // data to collect
    std::string include_guard;        // macro that protects this file, if any
    std::set<std::string> bad_macros; // misnamed macros in this file

    try {
        // create a fake main program in memory to include our header from
        std::string fake_main("#include \"");
        fake_main += filename;
        fake_main += "\"\n";

        typedef cpplexer::lex_token<> token_type;
        typedef cpplexer::lex_iterator<token_type> lex_iterator_type;
        typedef context<std::string::iterator, lex_iterator_type,
                        iteration_context_policies::load_file_to_string,
                        macroname_preprocessing_hooks>
            context_type;

        context_type ctx (fake_main.begin(), fake_main.end(), "in-memory.cpp",
                          macroname_preprocessing_hooks(re, bad_macros,
                                                        include_guard));

        // consume input, letting the hooks do the work
        context_type::iterator_type last = ctx.end();
        for (context_type::iterator_type it = ctx.begin(); it != last; ++it);

        std::set<std::string>::const_iterator beg = bad_macros.begin();
        std::set<std::string>::const_iterator end = bad_macros.end();
        if (beg != end) {
            // we have some macros that don't follow convention
            for (std::set<std::string>::const_iterator it = beg;
                 it != end; ++it) {
                std::cout << *it << " " << filename;
                if (*it == include_guard)
                    std::cout << " (guard)\n";
                else
                    std::cout << "\n";
            }
        }
    } catch (preprocess_exception const& e) {
        // some preprocessing error
        std::cerr
            << e.file_name() << "(" << e.line_no() << "): "
            << e.description() << std::endl;
    } catch (cpplexer::lexing_exception const& e) {
        std::cerr
            << e.file_name() << "(" << e.line_no() << "): "
            << e.description() << std::endl;
    }
}

///////////////////////////////////////////////////////////////////////////////
//  Main entry point
//
//  This sample shows how to check macros defined in header files to ensure
//  they conform to a standard naming convention. It uses hooks for
//  macro definition and include guard events to collect and report results.

int main(int argc, char *argv[])
{

    // argument processing
    namespace po = boost::program_options;
    po::options_description visible("Usage: check_macro_naming [options] directory");
    // named arguments
    visible.add_options()
        ("help,h", "print out options")
        ("match", po::value<std::string>()->default_value("^BOOST_.*"),
         "pattern defined macros must match")
        ("exclude", po::value<std::vector<std::string> >(), "subdirectory to skip");

    // positional arguments
    po::positional_options_description p;
    p.add("dirname", 1);
    // this positional option should not be displayed as a named, so we separate it:
    po::options_description hidden;
    hidden.add_options()("dirname", po::value<std::string>());

    // combine visible and hidden for parsing:
    po::options_description desc;
    desc.add(visible).add(hidden);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv)
              .options(desc)
              .positional(p)
              .run(),
              vm);
    po::notify(vm);

    if (vm.count("help") || (vm.count("dirname") == 0)) {
        std::cerr << visible << "\n";
        std::cerr << "recursively traverse directory, reporting macro definitions ";
        std::cerr << "that do not conform to the supplied pattern\n";
        return 1;
    }

    // get named parameters
    boost::regex macro_regex(vm["match"].as<std::string>());
    std::vector<std::string> exclude_dirnames;
    if (vm.count("exclude"))
        exclude_dirnames = vm["exclude"].as<std::vector<std::string> >();

    // get our single positional parameter - the directory to process
    std::string dirname = vm["dirname"].as<std::string>();

    // directory traversal logic
    static const boost::regex header_regex(".*\\.(hh|hpp|h)$");
    namespace fs = boost::filesystem;
    std::vector<fs::path> exclude_dirs(exclude_dirnames.begin(),
                                       exclude_dirnames.end());
    // canonicalize exclude directories for comparison vs.
    // search directories - either may be relative
    typedef fs::path (*canonicalizer)(fs::path const&, fs::path const&);
    using namespace boost::placeholders;
    std::transform(exclude_dirs.begin(), exclude_dirs.end(),
                   exclude_dirs.begin(),
                   boost::bind(static_cast<canonicalizer>(&fs::canonical),
                               _1, fs::current_path()));

    fs::recursive_directory_iterator dir_end;
    fs::recursive_directory_iterator dir_beg(dirname);
    for (fs::recursive_directory_iterator it = dir_beg; it != dir_end; ++it) {
        if (it->status().type() == fs::regular_file) {
            std::string fn = it->path().string();
            if (regex_match(fn, header_regex))
                process_header(fn, macro_regex);
        }
        if ((it->status().type() == fs::directory_file) &&
            (std::find(exclude_dirs.begin(),
                       exclude_dirs.end(),
                       fs::canonical(it->path())) != exclude_dirs.end())) {
            // skip recursion here
            it.no_push();
        }
    }

    return 0;
}
