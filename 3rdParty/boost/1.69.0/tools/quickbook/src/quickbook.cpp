/*=============================================================================
    Copyright (c) 2002 2004 2006 Joel de Guzman
    Copyright (c) 2004 Eric Niebler
    http://spirit.sourceforge.net/

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include "quickbook.hpp"
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/program_options.hpp>
#include <boost/range/algorithm/replace.hpp>
#include <boost/range/algorithm/transform.hpp>
#include <boost/ref.hpp>
#include <boost/version.hpp>
#include "actions.hpp"
#include "bb2html.hpp"
#include "document_state.hpp"
#include "files.hpp"
#include "for.hpp"
#include "grammar.hpp"
#include "path.hpp"
#include "post_process.hpp"
#include "state.hpp"
#include "stream.hpp"
#include "utils.hpp"

#include <iterator>
#include <stdexcept>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif

#if (defined(BOOST_MSVC) && (BOOST_MSVC <= 1310))
#pragma warning(disable : 4355)
#endif

#define QUICKBOOK_VERSION "Quickbook Version 1.7.2"

namespace quickbook
{
    namespace cl = boost::spirit::classic;
    namespace fs = boost::filesystem;

    tm* current_time;    // the current time
    tm* current_gm_time; // the current UTC time
    bool debug_mode;     // for quickbook developers only
    bool self_linked_headers;
    std::vector<fs::path> include_path;
    std::vector<std::string> preset_defines;
    fs::path image_location;

    static void set_macros(quickbook::state& state)
    {
        QUICKBOOK_FOR (quickbook::string_view val, preset_defines) {
            parse_iterator first(val.begin());
            parse_iterator last(val.end());

            cl::parse_info<parse_iterator> info =
                cl::parse(first, last, state.grammar().command_line_macro);

            if (!info.full) {
                detail::outerr() << "Error parsing command line definition: '"
                                 << val << "'" << std::endl;
                ++state.error_count;
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////
    //
    //  Parse a file
    //
    ///////////////////////////////////////////////////////////////////////////
    void parse_file(
        quickbook::state& state, value include_doc_id, bool nested_file)
    {
        parse_iterator first(state.current_file->source().begin());
        parse_iterator last(state.current_file->source().end());

        cl::parse_info<parse_iterator> info =
            cl::parse(first, last, state.grammar().doc_info);
        assert(info.hit);

        if (!state.error_count) {
            std::string doc_type =
                pre(state, info.stop, include_doc_id, nested_file);

            info = cl::parse(
                info.hit ? info.stop : first, last,
                state.grammar().block_start);

            post(state, doc_type);

            if (!info.full) {
                file_position const& pos =
                    state.current_file->position_of(info.stop.base());
                detail::outerr(state.current_file->path, pos.line)
                    << "Syntax Error near column " << pos.column << ".\n";
                ++state.error_count;
            }
        }
    }

    struct parse_document_options
    {
        enum output_format
        {
            boostbook,
            html
        };
        enum output_style
        {
            output_none = 0,
            output_file,
            output_chunked
        };

        parse_document_options()
            : format(boostbook)
            , style(output_file)
            , output_path()
            , indent(-1)
            , linewidth(-1)
            , pretty_print(true)
            , strict_mode(false)
            , deps_out_flags(quickbook::dependency_tracker::default_)
        {
        }

        output_format format;
        output_style style;
        fs::path output_path;
        int indent;
        int linewidth;
        bool pretty_print;
        bool strict_mode;
        fs::path deps_out;
        quickbook::dependency_tracker::flags deps_out_flags;
        fs::path locations_out;
        fs::path xinclude_base;
        quickbook::detail::html_options html_ops;
    };

    static int parse_document(
        fs::path const& filein_, parse_document_options const& options_)
    {
        string_stream buffer;
        document_state output;

        int result = 0;

        try {
            quickbook::state state(
                filein_, options_.xinclude_base, buffer, output);
            state.strict_mode = options_.strict_mode;
            set_macros(state);

            if (state.error_count == 0) {
                state.dependencies.add_dependency(filein_);
                state.current_file = load(filein_); // Throws load_error

                parse_file(state);

                if (state.error_count) {
                    detail::outerr()
                        << "Error count: " << state.error_count << ".\n";
                }
            }

            result = state.error_count ? 1 : 0;

            if (!options_.deps_out.empty()) {
                state.dependencies.write_dependencies(
                    options_.deps_out, options_.deps_out_flags);
            }

            if (!options_.locations_out.empty()) {
                fs::ofstream out(options_.locations_out);
                state.dependencies.write_dependencies(
                    options_.locations_out, dependency_tracker::checked);
            }
        } catch (load_error& e) {
            detail::outerr(filein_) << e.what() << std::endl;
            result = 1;
        } catch (std::runtime_error& e) {
            detail::outerr() << e.what() << std::endl;
            result = 1;
        }

        if (result) {
            return result;
        }

        if (options_.style) {
            std::string stage2 = output.replace_placeholders(buffer.str());

            if (options_.pretty_print) {
                try {
                    stage2 = post_process(
                        stage2, options_.indent, options_.linewidth);
                } catch (quickbook::post_process_failure&) {
                    ::quickbook::detail::outerr()
                        << "Post Processing Failed." << std::endl;
                    if (options_.format == parse_document_options::boostbook) {
                        // Can still write out a boostbook file, but return an
                        // error code.
                        result = 1;
                    }
                    else {
                        return 1;
                    }
                }
            }

            if (options_.format == parse_document_options::html) {
                if (result) {
                    return result;
                }
                return quickbook::detail::boostbook_to_html(
                    stage2, options_.html_ops);
            }
            else {
                fs::ofstream fileout(options_.output_path);

                if (fileout.fail()) {
                    ::quickbook::detail::outerr()
                        << "Error opening output file " << options_.output_path
                        << std::endl;

                    return 1;
                }

                fileout << stage2;

                if (fileout.fail()) {
                    ::quickbook::detail::outerr()
                        << "Error writing to output file "
                        << options_.output_path << std::endl;

                    return 1;
                }
            }
        }

        return result;
    }
}

///////////////////////////////////////////////////////////////////////////
//
//  Main program
//
///////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
    try {
        namespace fs = boost::filesystem;
        namespace po = boost::program_options;

        using boost::program_options::options_description;
        using boost::program_options::variables_map;
        using boost::program_options::store;
        using boost::program_options::parse_command_line;
        using boost::program_options::wcommand_line_parser;
        using boost::program_options::command_line_parser;
        using boost::program_options::notify;
        using boost::program_options::positional_options_description;

        using namespace quickbook;
        using quickbook::detail::command_line_string;

        // First thing, the filesystem should record the current working
        // directory.
        fs::initial_path<fs::path>();

        // Various initialisation methods
        quickbook::detail::initialise_output();
        quickbook::detail::initialise_markups();

        // Declare the program options

        options_description desc("Allowed options");
        options_description html_desc("HTML options");
        options_description hidden("Hidden options");
        options_description all("All options");

#if QUICKBOOK_WIDE_PATHS
#define PO_VALUE po::wvalue
#else
#define PO_VALUE po::value
#endif

        // clang-format off

        desc.add_options()
            ("help", "produce help message")
            ("version", "print version string")
            ("no-pretty-print", "disable XML pretty printing")
            ("strict", "strict mode")
            ("no-self-linked-headers", "stop headers linking to themselves")
            ("indent", PO_VALUE<int>(), "indent spaces")
            ("linewidth", PO_VALUE<int>(), "line width")
            ("input-file", PO_VALUE<command_line_string>(), "input file")
            ("output-format", PO_VALUE<command_line_string>(), "boostbook, html, onehtml")
            ("output-file", PO_VALUE<command_line_string>(), "output file (for boostbook or onehtml)")
            ("output-dir", PO_VALUE<command_line_string>(), "output directory (for html)")
            ("no-output", "don't write out the result")
            ("output-deps", PO_VALUE<command_line_string>(), "output dependency file")
            ("ms-errors", "use Microsoft Visual Studio style error & warn message format")
            ("include-path,I", PO_VALUE< std::vector<command_line_string> >(), "include path")
            ("define,D", PO_VALUE< std::vector<command_line_string> >(), "define macro")
            ("image-location", PO_VALUE<command_line_string>(), "image location")
        ;

        html_desc.add_options()
            ("boost-root-path", PO_VALUE<command_line_string>(), "boost root (file path or absolute URL)")
            ("css-path", PO_VALUE<command_line_string>(), "css file (file path or absolute URL)")
            ("graphics-path", PO_VALUE<command_line_string>(), "graphics directory (file path or absolute URL)");
        desc.add(html_desc);

        hidden.add_options()
            ("debug", "debug mode")
            ("expect-errors",
                "Succeed if the input file contains a correctly handled "
                "error, fail otherwise.")
            ("xinclude-base", PO_VALUE<command_line_string>(),
                "Generate xincludes as if generating for this target "
                "directory.")
            ("output-deps-format", PO_VALUE<command_line_string>(),
             "Comma separated list of formatting options for output-deps, "
             "options are: escaped, checked")
            ("output-checked-locations", PO_VALUE<command_line_string>(),
             "Writes a file listing all the file locations that were "
             "checked, starting with '+' if they were found, or '-' "
             "if they weren't.\n"
             "This is deprecated, use 'output-deps-format=checked' to "
             "write the deps file in this format.")
        ;

        // clang-format on

        all.add(desc).add(hidden);

        positional_options_description p;
        p.add("input-file", -1);

        // Read option from the command line

        variables_map vm;

#if QUICKBOOK_WIDE_PATHS
        quickbook::ignore_variable(&argc);
        quickbook::ignore_variable(&argv);

        int wide_argc;
        LPWSTR* wide_argv = CommandLineToArgvW(GetCommandLineW(), &wide_argc);
        if (!wide_argv) {
            quickbook::detail::outerr()
                << "Error getting argument values." << std::endl;
            return 1;
        }

        store(
            wcommand_line_parser(wide_argc, wide_argv)
                .options(all)
                .positional(p)
                .run(),
            vm);

        LocalFree(wide_argv);
#else
        store(
            command_line_parser(argc, argv).options(all).positional(p).run(),
            vm);
#endif

        notify(vm);

        // Process the command line options

        parse_document_options options;
        bool expect_errors = vm.count("expect-errors");
        int error_count = 0;
        bool output_specified = false;
        bool alt_output_specified = false;

        if (vm.count("help")) {
            std::ostringstream description_text;
            description_text << desc;

            quickbook::detail::out() << description_text.str() << "\n";

            return 0;
        }

        if (vm.count("version")) {
            std::string boost_version = BOOST_LIB_VERSION;
            boost::replace(boost_version, '_', '.');

            quickbook::detail::out() << QUICKBOOK_VERSION << " (Boost "
                                     << boost_version << ")" << std::endl;
            return 0;
        }

        quickbook::detail::set_ms_errors(vm.count("ms-errors"));

        if (vm.count("no-pretty-print")) options.pretty_print = false;

        options.strict_mode = !!vm.count("strict");

        if (vm.count("indent")) options.indent = vm["indent"].as<int>();

        if (vm.count("linewidth"))
            options.linewidth = vm["linewidth"].as<int>();

        if (vm.count("output-format")) {
            output_specified = true;
            std::string format = quickbook::detail::command_line_to_utf8(
                vm["output-format"].as<command_line_string>());
            if (format == "html") {
                options.format = quickbook::parse_document_options::html;
                options.style =
                    quickbook::parse_document_options::output_chunked;
            }
            else if (format == "onehtml") {
                options.format = quickbook::parse_document_options::html;
                options.style = quickbook::parse_document_options::output_file;
            }
            else if (format == "boostbook") {
                options.format = quickbook::parse_document_options::boostbook;
                options.style = quickbook::parse_document_options::output_file;
            }
            else {
                quickbook::detail::outerr()
                    << "Unknown output format: " << format << std::endl;

                ++error_count;
            }
        }

        quickbook::self_linked_headers =
            options.format != parse_document_options::html &&
            !vm.count("no-self-linked-headers");

        if (vm.count("debug")) {
            static tm timeinfo;
            timeinfo.tm_year = 2000 - 1900;
            timeinfo.tm_mon = 12 - 1;
            timeinfo.tm_mday = 20;
            timeinfo.tm_hour = 12;
            timeinfo.tm_min = 0;
            timeinfo.tm_sec = 0;
            timeinfo.tm_isdst = -1;
            mktime(&timeinfo);
            quickbook::current_time = &timeinfo;
            quickbook::current_gm_time = &timeinfo;
            quickbook::debug_mode = true;
        }
        else {
            time_t t = std::time(0);
            static tm lt = *localtime(&t);
            static tm gmt = *gmtime(&t);
            quickbook::current_time = &lt;
            quickbook::current_gm_time = &gmt;
            quickbook::debug_mode = false;
        }

        quickbook::include_path.clear();
        if (vm.count("include-path")) {
            boost::transform(
                vm["include-path"].as<std::vector<command_line_string> >(),
                std::back_inserter(quickbook::include_path),
                quickbook::detail::command_line_to_path);
        }

        quickbook::preset_defines.clear();
        if (vm.count("define")) {
            boost::transform(
                vm["define"].as<std::vector<command_line_string> >(),
                std::back_inserter(quickbook::preset_defines),
                quickbook::detail::command_line_to_utf8);
        }

        if (vm.count("input-file")) {
            fs::path filein = quickbook::detail::command_line_to_path(
                vm["input-file"].as<command_line_string>());

            if (!fs::exists(filein)) {
                quickbook::detail::outerr()
                    << "file not found: " << filein << std::endl;
                ++error_count;
            }

            if (vm.count("output-deps")) {
                alt_output_specified = true;
                options.deps_out = quickbook::detail::command_line_to_path(
                    vm["output-deps"].as<command_line_string>());
            }

            if (vm.count("output-deps-format")) {
                std::string format_flags =
                    quickbook::detail::command_line_to_utf8(
                        vm["output-deps-format"].as<command_line_string>());

                std::vector<std::string> flag_names;
                boost::algorithm::split(
                    flag_names, format_flags, boost::algorithm::is_any_of(", "),
                    boost::algorithm::token_compress_on);

                unsigned flags = 0;

                QUICKBOOK_FOR (std::string const& flag, flag_names) {
                    if (flag == "checked") {
                        flags |= quickbook::dependency_tracker::checked;
                    }
                    else if (flag == "escaped") {
                        flags |= quickbook::dependency_tracker::escaped;
                    }
                    else if (!flag.empty()) {
                        quickbook::detail::outerr()
                            << "Unknown dependency format flag: " << flag
                            << std::endl;

                        ++error_count;
                    }
                }

                options.deps_out_flags =
                    quickbook::dependency_tracker::flags(flags);
            }

            if (vm.count("output-checked-locations")) {
                alt_output_specified = true;
                options.locations_out = quickbook::detail::command_line_to_path(
                    vm["output-checked-locations"].as<command_line_string>());
            }

            if (vm.count("boost-root-path")) {
                // TODO: Check that it's a directory?
                options.html_ops.boost_root_path =
                    vm["boost-root-path"].as<command_line_string>();
            }
            // Could possibly default it:
            // 'boost:' links will use this anyway, but setting a default
            // would also result in default css and graphics paths.
            //
            // else {
            //    options.html_ops.boost_root_path =
            //      quickbook::detail::path_or_url::url(
            //        "http://www.boost.org/doc/libs/release/");
            //}

            if (vm.count("css-path")) {
                options.html_ops.css_path =
                    vm["css-path"].as<command_line_string>();
            }
            else if (options.html_ops.boost_root_path) {
                options.html_ops.css_path =
                    options.html_ops.boost_root_path / "doc/src/boostbook.css";
            }

            if (vm.count("graphics-path")) {
                options.html_ops.graphics_path =
                    vm["graphics-path"].as<command_line_string>();
            }
            else if (options.html_ops.boost_root_path) {
                options.html_ops.graphics_path =
                    options.html_ops.boost_root_path / "doc/src/images";
            }

            if (vm.count("output-file")) {
                output_specified = true;
                switch (options.style) {
                case quickbook::parse_document_options::output_file: {
                    options.output_path =
                        quickbook::detail::command_line_to_path(
                            vm["output-file"].as<command_line_string>());

                    fs::path parent = options.output_path.parent_path();
                    if (!parent.empty() && !fs::is_directory(parent)) {
                        quickbook::detail::outerr()
                            << "parent directory not found for output file"
                            << std::endl;
                        ++error_count;
                    }
                    break;
                }
                case quickbook::parse_document_options::output_chunked:
                    quickbook::detail::outerr()
                        << "output-file give for chunked output" << std::endl;
                    ++error_count;
                    break;
                case quickbook::parse_document_options::output_none:
                    quickbook::detail::outerr()
                        << "output-file given for no output" << std::endl;
                    ++error_count;
                    break;
                default:
                    assert(false);
                }
            }

            if (vm.count("output-dir")) {
                output_specified = true;
                switch (options.style) {
                case quickbook::parse_document_options::output_chunked: {
                    options.output_path =
                        quickbook::detail::command_line_to_path(
                            vm["output-dir"].as<command_line_string>());

                    if (!fs::is_directory(options.output_path.parent_path())) {
                        quickbook::detail::outerr()
                            << "parent directory not found for output directory"
                            << std::endl;
                        ++error_count;
                    }
                }
                case quickbook::parse_document_options::output_file:
                    quickbook::detail::outerr()
                        << "output-dir give for file output" << std::endl;
                    ++error_count;
                    break;
                case quickbook::parse_document_options::output_none:
                    quickbook::detail::outerr()
                        << "output-dir given for no output" << std::endl;
                    ++error_count;
                    break;
                default:
                    assert(false);
                }
            }

            if (!vm.count("output-file") && !vm.count("output-dir")) {
                if (!output_specified && alt_output_specified) {
                    options.style =
                        quickbook::parse_document_options::output_none;
                }
                else {
                    fs::path path = filein;
                    switch (options.style) {
                    case quickbook::parse_document_options::output_chunked:
                        path = path.parent_path() / "html";
                        options.style =
                            quickbook::parse_document_options::output_chunked;
                        options.output_path = path;
                        break;
                    case quickbook::parse_document_options::output_file:
                        switch (options.format) {
                        case quickbook::parse_document_options::html:
                            path.replace_extension(".html");
                            break;
                        case quickbook::parse_document_options::boostbook:
                            path.replace_extension(".xml");
                            break;
                        default:
                            assert(false);
                            path.replace_extension(".xml");
                        }
                        options.output_path = path;
                        break;
                    default:
                        assert(false);
                        options.style =
                            quickbook::parse_document_options::output_none;
                    }
                }
            }

            if (vm.count("xinclude-base")) {
                options.xinclude_base = quickbook::detail::command_line_to_path(
                    vm["xinclude-base"].as<command_line_string>());

                // I'm not sure if this error check is necessary.
                // There might be valid reasons to use a path that doesn't
                // exist yet, or a path that just generates valid relative
                // paths.
                if (!fs::is_directory(options.xinclude_base)) {
                    quickbook::detail::outerr()
                        << "xinclude-base is not a directory" << std::endl;
                    ++error_count;
                }
            }
            else {
                options.xinclude_base =
                    options.style == parse_document_options::output_chunked
                        ? options.output_path
                        : options.output_path.parent_path();
                if (options.xinclude_base.empty()) {
                    options.xinclude_base = ".";
                }

                // If output_path was implicitly created from filein, then it
                // should be in filein's directory.
                // If output_path was explicitly specified, then it's already
                // been checked.
                assert(error_count || fs::is_directory(options.xinclude_base));
            }

            if (vm.count("image-location")) {
                quickbook::image_location =
                    quickbook::detail::command_line_to_path(
                        vm["image-location"].as<command_line_string>());
            }
            else {
                quickbook::image_location = filein.parent_path() / "html";
            }

            // Set duplicated html_options.
            // TODO: Clean this up?
            if (options.style == parse_document_options::output_chunked) {
                options.html_ops.home_path = options.output_path / "index.html";
                options.html_ops.chunked_output = true;
            }
            else {
                options.html_ops.home_path = options.output_path;
                options.html_ops.chunked_output = false;
            }
            options.html_ops.pretty_print = options.pretty_print;

            if (!error_count) {
                switch (options.style) {
                case parse_document_options::output_file:
                    quickbook::detail::out()
                        << "Generating output file: " << options.output_path
                        << std::endl;
                    break;
                case parse_document_options::output_chunked:
                    quickbook::detail::out()
                        << "Generating output path: " << options.output_path
                        << std::endl;
                    break;
                case parse_document_options::output_none:
                    break;
                default:
                    assert(false);
                }

                error_count += quickbook::parse_document(filein, options);
            }

            if (expect_errors) {
                if (!error_count)
                    quickbook::detail::outerr()
                        << "No errors detected for --expect-errors."
                        << std::endl;
                return !error_count;
            }
            else {
                return error_count;
            }
        }
        else {
            std::ostringstream description_text;
            description_text << desc;

            quickbook::detail::outerr() << "No filename given\n\n"
                                        << description_text.str() << std::endl;
            return 1;
        }
    }

    catch (std::exception& e) {
        quickbook::detail::outerr() << e.what() << "\n";
        return 1;
    }

    catch (...) {
        quickbook::detail::outerr() << "Exception of unknown type caught\n";
        return 1;
    }

    return 0;
}
