
// boostdep - a tool to generate Boost dependency reports
//
// Copyright 2014, 2015, 2016 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <climits>

namespace fs = boost::filesystem;

// header -> module
static std::map< std::string, std::string > s_header_map;

// module -> headers
static std::map< std::string, std::set<std::string> > s_module_headers;

static std::set< std::string > s_modules;

static void scan_module_headers( fs::path const & path )
{
    try
    {
        std::string module = path.generic_string().substr( 5 ); // strip "libs/"

        std::replace( module.begin(), module.end(), '/', '~' );

        s_modules.insert( module );

        fs::path dir = path / "include";
        size_t n = dir.generic_string().size();

        fs::recursive_directory_iterator it( dir ), last;

        for( ; it != last; ++it )
        {
            fs::directory_entry const & e = *it;

            std::string p2 = e.path().generic_string();
            p2 = p2.substr( n+1 );

            // std::cout << module << ": " << p2 << std::endl;

            s_header_map[ p2 ] = module;
            s_module_headers[ module ].insert( p2 );
        }
    }
    catch( fs::filesystem_error const & x )
    {
        std::cout << x.what() << std::endl;
    }
}

static void scan_submodules( fs::path const & path )
{
    fs::directory_iterator it( path ), last;

    for( ; it != last; ++it )
    {
        fs::directory_entry const & e = *it;

        if( e.status().type() != fs::directory_file )
        {
            continue;
        }

        fs::path path = e.path();

        if( fs::exists( path / "include" ) )
        {
            scan_module_headers( path );
        }

        if( fs::exists( path / "sublibs" ) )
        {
            scan_submodules( path );
        }
    }
}

static void build_header_map()
{
    scan_submodules( "libs" );
}

static void scan_header_dependencies( std::string const & header, std::istream & is, std::map< std::string, std::set< std::string > > & deps, std::map< std::string, std::set< std::string > > & from )
{
    std::string line;

    while( std::getline( is, line ) )
    {
        while( !line.empty() && ( line[0] == ' ' || line[0] == '\t' ) )
        {
            line.erase( 0, 1 );
        }

        if( line.empty() || line[0] != '#' ) continue;

        line.erase( 0, 1 );

        while( !line.empty() && ( line[0] == ' ' || line[0] == '\t' ) )
        {
            line.erase( 0, 1 );
        }

        if( line.substr( 0, 7 ) != "include" ) continue;

        line.erase( 0, 7 );

        while( !line.empty() && ( line[0] == ' ' || line[0] == '\t' ) )
        {
            line.erase( 0, 1 );
        }

        if( line.size() < 2 ) continue;

        char ch = line[0];

        if( ch != '<' && ch != '"' ) continue;

        if( ch == '<' )
        {
            ch = '>';
        }

        line.erase( 0, 1 );

        std::string::size_type k = line.find_first_of( ch );

        if( k != std::string::npos )
        {
            line.erase( k );
        }

        std::map< std::string, std::string >::const_iterator i = s_header_map.find( line );

        if( i != s_header_map.end() )
        {
            deps[ i->second ].insert( line );
            from[ line ].insert( header );
        }
        else if( line.substr( 0, 6 ) == "boost/" )
        {
            deps[ "(unknown)" ].insert( line );
            from[ line ].insert( header );
        }
    }
}

struct module_primary_actions
{
    virtual void heading( std::string const & module ) = 0;

    virtual void module_start( std::string const & module ) = 0;
    virtual void module_end( std::string const & module ) = 0;

    virtual void header_start( std::string const & header ) = 0;
    virtual void header_end( std::string const & header ) = 0;

    virtual void from_header( std::string const & header ) = 0;
};

static fs::path module_include_path( std::string module )
{
    std::replace( module.begin(), module.end(), '~', '/' );
    return fs::path( "libs" ) / module / "include";
}

static fs::path module_source_path( std::string module )
{
    std::replace( module.begin(), module.end(), '~', '/' );
    return fs::path( "libs" ) / module / "src";
}

static fs::path module_build_path( std::string module )
{
    std::replace( module.begin(), module.end(), '~', '/' );
    return fs::path( "libs" ) / module / "build";
}

static void scan_module_dependencies( std::string const & module, module_primary_actions & actions, bool track_sources, bool include_self )
{
    // module -> [ header, header... ]
    std::map< std::string, std::set< std::string > > deps;

    // header -> included from [ header, header... ]
    std::map< std::string, std::set< std::string > > from;

    {
        fs::path dir = module_include_path( module );
        size_t n = dir.generic_string().size();

        fs::recursive_directory_iterator it( dir ), last;

        for( ; it != last; ++it )
        {
            std::string header = it->path().generic_string().substr( n+1 );

            fs::ifstream is( it->path() );

            scan_header_dependencies( header, is, deps, from );
        }
    }

    if( track_sources )
    {
        fs::path dir = module_source_path( module );
        size_t n = dir.generic_string().size();

        if( fs::exists( dir ) )
        {
            fs::recursive_directory_iterator it( dir ), last;

            for( ; it != last; ++it )
            {
                std::string header = it->path().generic_string().substr( n+1 );

                fs::ifstream is( it->path() );

                scan_header_dependencies( header, is, deps, from );
            }
        }
    }

    actions.heading( module );

    for( std::map< std::string, std::set< std::string > >::iterator i = deps.begin(); i != deps.end(); ++i )
    {
        if( i->first == module && !include_self ) continue;

        actions.module_start( i->first );

        for( std::set< std::string >::iterator j = i->second.begin(); j != i->second.end(); ++j )
        {
            actions.header_start( *j );

            std::set< std::string > const & f = from[ *j ];

            for( std::set< std::string >::const_iterator k = f.begin(); k != f.end(); ++k )
            {
                actions.from_header( *k );
            }

            actions.header_end( *j );
        }

        actions.module_end( i->first );
    }
}

// module depends on [ module, module... ]
static std::map< std::string, std::set< std::string > > s_module_deps;

// header is included by [header, header...]
static std::map< std::string, std::set< std::string > > s_header_deps;

// [ module, module... ] depend on module
static std::map< std::string, std::set< std::string > > s_reverse_deps;

// header includes [header, header...]
static std::map< std::string, std::set< std::string > > s_header_includes;

struct build_mdmap_actions: public module_primary_actions
{
    std::string module_;
    std::string module2_;
    std::string header_;

    void heading( std::string const & module )
    {
        module_ = module;
    }

    void module_start( std::string const & module )
    {
        if( module != module_ )
        {
            s_module_deps[ module_ ].insert( module );
            s_reverse_deps[ module ].insert( module_ );
        }

        module2_ = module;
    }

    void module_end( std::string const & /*module*/ )
    {
    }

    void header_start( std::string const & header )
    {
        header_ = header;
    }

    void header_end( std::string const & /*header*/ )
    {
    }

    void from_header( std::string const & header )
    {
        if( module_ != module2_ )
        {
            s_header_deps[ header_ ].insert( header );
        }

        s_header_includes[ header ].insert( header_ );
    }
};

static void build_module_dependency_map( bool track_sources )
{
    for( std::set< std::string >::iterator i = s_modules.begin(); i != s_modules.end(); ++i )
    {
        build_mdmap_actions actions;
        scan_module_dependencies( *i, actions, track_sources, true );
    }
}

static void output_module_primary_report( std::string const & module, module_primary_actions & actions, bool track_sources )
{
    try
    {
        scan_module_dependencies( module, actions, track_sources, false );
    }
    catch( fs::filesystem_error const & x )
    {
        std::cout << x.what() << std::endl;
    }
}

struct module_secondary_actions
{
    virtual void heading( std::string const & module ) = 0;

    virtual void module_start( std::string const & module ) = 0;
    virtual void module_end( std::string const & module ) = 0;

    virtual void module_adds( std::string const & module ) = 0;
};

static void exclude( std::set< std::string > & x, std::set< std::string > const & y )
{
    for( std::set< std::string >::const_iterator i = y.begin(); i != y.end(); ++i )
    {
        x.erase( *i );
    }
}

static void output_module_secondary_report( std::string const & module, module_secondary_actions & actions )
{
    actions.heading( module );

    std::set< std::string > deps = s_module_deps[ module ];

    deps.insert( module );

    // build transitive closure

    for( ;; )
    {
        std::set< std::string > deps2( deps );

        for( std::set< std::string >::iterator i = deps.begin(); i != deps.end(); ++i )
        {
            std::set< std::string > deps3 = s_module_deps[ *i ];

            exclude( deps3, deps );

            if( deps3.empty() )
            {
                continue;
            }

            actions.module_start( *i );

            for( std::set< std::string >::iterator j = deps3.begin(); j != deps3.end(); ++j )
            {
                actions.module_adds( *j );
            }

            actions.module_end( *i );

            deps2.insert( deps3.begin(), deps3.end() );
        }

        if( deps == deps2 )
        {
            break;
        }
        else
        {
            deps = deps2;
        }
    }
}

struct header_inclusion_actions
{
    virtual void heading( std::string const & header, std::string const & module ) = 0;

    virtual void module_start( std::string const & module ) = 0;
    virtual void module_end( std::string const & module ) = 0;

    virtual void header( std::string const & header ) = 0;
};

static void output_header_inclusion_report( std::string const & header, header_inclusion_actions & actions )
{
    std::string module = s_header_map[ header ];

    actions.heading( header, module );

    std::set< std::string > from = s_header_deps[ header ];

    // classify 'from' dependencies by module

    // module -> [header, header...]
    std::map< std::string, std::set< std::string > > from2;

    for( std::set< std::string >::iterator i = from.begin(); i != from.end(); ++i )
    {
        from2[ s_header_map[ *i ] ].insert( *i );
    }

    for( std::map< std::string, std::set< std::string > >::iterator i = from2.begin(); i != from2.end(); ++i )
    {
        actions.module_start( i->first );

        for( std::set< std::string >::iterator j = i->second.begin(); j != i->second.end(); ++j )
        {
            actions.header( *j );
        }

        actions.module_end( i->first );
    }
}

// output_module_primary_report

struct module_primary_txt_actions: public module_primary_actions
{
    void heading( std::string const & module )
    {
        std::cout << "Primary dependencies for " << module << ":\n\n";
    }

    void module_start( std::string const & module )
    {
        std::cout << module << ":\n";
    }

    void module_end( std::string const & /*module*/ )
    {
        std::cout << "\n";
    }

    void header_start( std::string const & header )
    {
        std::cout << "    <" << header << ">\n";
    }

    void header_end( std::string const & /*header*/ )
    {
    }

    void from_header( std::string const & header )
    {
        std::cout << "        from <" << header << ">\n";
    }
};

struct module_primary_html_actions: public module_primary_actions
{
    void heading( std::string const & module )
    {
        std::cout << "\n\n<h1 id=\"primary-dependencies\">Primary dependencies for <em>" << module << "</em></h1>\n";
    }

    void module_start( std::string const & module )
    {
        std::cout << "  <h2 id=\"" << module << "\"><a href=\"" << module << ".html\"><em>" << module << "</em></a></h2>\n";
    }

    void module_end( std::string const & /*module*/ )
    {
    }

    void header_start( std::string const & header )
    {
        std::cout << "    <h3><code>&lt;" << header << "&gt;</code></h3><ul>\n";
    }

    void header_end( std::string const & /*header*/ )
    {
        std::cout << "    </ul>\n";
    }

    void from_header( std::string const & header )
    {
        std::cout << "      <li>from <code>&lt;" << header << "&gt;</code></li>\n";
    }
};

static void output_module_primary_report( std::string const & module, bool html, bool track_sources )
{
    if( html )
    {
        module_primary_html_actions actions;
        output_module_primary_report( module, actions, track_sources );
    }
    else
    {
        module_primary_txt_actions actions;
        output_module_primary_report( module, actions, track_sources );
    }
}

// output_module_secondary_report

struct module_secondary_txt_actions: public module_secondary_actions
{
    void heading( std::string const & module )
    {
        std::cout << "Secondary dependencies for " << module << ":\n\n";
    }

    void module_start( std::string const & module )
    {
        std::cout << module << ":\n";
    }

    void module_end( std::string const & /*module*/ )
    {
        std::cout << "\n";
    }

    void module_adds( std::string const & module )
    {
        std::cout << "    adds " << module << "\n";
    }
};

struct module_secondary_html_actions: public module_secondary_actions
{
    std::string m2_;

    void heading( std::string const & module )
    {
        std::cout << "\n\n<h1 id=\"secondary-dependencies\">Secondary dependencies for <em>" << module << "</em></h1>\n";
    }

    void module_start( std::string const & module )
    {
        std::cout << "  <h2><a href=\"" << module << ".html\"><em>" << module << "</em></a></h2><ul>\n";
        m2_ = module;
    }

    void module_end( std::string const & /*module*/ )
    {
        std::cout << "  </ul>\n";
    }

    void module_adds( std::string const & module )
    {
        std::cout << "    <li><a href=\"" << m2_ << ".html#" << module << "\">adds <em>" << module << "</em></a></li>\n";
    }
};

static void output_module_secondary_report( std::string const & module, bool html )
{
    if( html )
    {
        module_secondary_html_actions actions;
        output_module_secondary_report( module, actions );
    }
    else
    {
        module_secondary_txt_actions actions;
        output_module_secondary_report( module, actions );
    }
}

// output_header_report

struct header_inclusion_txt_actions: public header_inclusion_actions
{
    void heading( std::string const & header, std::string const & module )
    {
        std::cout << "Inclusion report for <" << header << "> (in module " << module << "):\n\n";
    }

    void module_start( std::string const & module )
    {
        std::cout << "    from " << module << ":\n";
    }

    void module_end( std::string const & /*module*/ )
    {
        std::cout << "\n";
    }

    void header( std::string const & header )
    {
        std::cout << "        <" << header << ">\n";
    }
};

struct header_inclusion_html_actions: public header_inclusion_actions
{
    void heading( std::string const & header, std::string const & module )
    {
        std::cout << "<h1>Inclusion report for <code>&lt;" << header << "&gt;</code> (in module <em>" << module << "</em>)</h1>\n";
    }

    void module_start( std::string const & module )
    {
        std::cout << "  <h2>From <a href=\"" << module << ".html\"><em>" << module << "</em></a></h2><ul>\n";
    }

    void module_end( std::string const & /*module*/ )
    {
        std::cout << "  </ul>\n";
    }

    void header( std::string const & header )
    {
        std::cout << "    <li><code>&lt;" << header << "&gt;</code></li>\n";
    }
};

static void output_header_report( std::string const & header, bool html )
{
    if( html )
    {
        header_inclusion_html_actions actions;
        output_header_inclusion_report( header, actions );
    }
    else
    {
        header_inclusion_txt_actions actions;
        output_header_inclusion_report( header, actions );
    }
}

// output_module_reverse_report

struct module_reverse_actions
{
    virtual void heading( std::string const & module ) = 0;

    virtual void module_start( std::string const & module ) = 0;
    virtual void module_end( std::string const & module ) = 0;

    virtual void header_start( std::string const & header ) = 0;
    virtual void header_end( std::string const & header ) = 0;

    virtual void from_header( std::string const & header ) = 0;
};

static void output_module_reverse_report( std::string const & module, module_reverse_actions & actions )
{
    actions.heading( module );

    std::set< std::string > const from = s_reverse_deps[ module ];

    for( std::set< std::string >::const_iterator i = from.begin(); i != from.end(); ++i )
    {
        actions.module_start( *i );

        for( std::map< std::string, std::set< std::string > >::iterator j = s_header_deps.begin(); j != s_header_deps.end(); ++j )
        {
            if( s_header_map[ j->first ] == module )
            {
                bool header_started = false;

                for( std::set< std::string >::iterator k = j->second.begin(); k != j->second.end(); ++k )
                {
                    if( s_header_map[ *k ] == *i )
                    {
                        if( !header_started )
                        {
                            actions.header_start( j->first );

                            header_started = true;
                        }

                        actions.from_header( *k );
                    }
                }

                if( header_started )
                {
                    actions.header_end( j->first );
                }
            }
        }

        actions.module_end( *i );
    }
}

struct module_reverse_txt_actions: public module_reverse_actions
{
    void heading( std::string const & module )
    {
        std::cout << "Reverse dependencies for " << module << ":\n\n";
    }

    void module_start( std::string const & module )
    {
        std::cout << module << ":\n";
    }

    void module_end( std::string const & /*module*/ )
    {
        std::cout << "\n";
    }

    void header_start( std::string const & header )
    {
        std::cout << "    <" << header << ">\n";
    }

    void header_end( std::string const & /*header*/ )
    {
    }

    void from_header( std::string const & header )
    {
        std::cout << "        from <" << header << ">\n";
    }
};

struct module_reverse_html_actions: public module_reverse_actions
{
    void heading( std::string const & module )
    {
        std::cout << "\n\n<h1 id=\"reverse-dependencies\">Reverse dependencies for <em>" << module << "</em></h1>\n";
    }

    void module_start( std::string const & module )
    {
        std::cout << "  <h2 id=\"reverse-" << module << "\"><a href=\"" << module << ".html\"><em>" << module << "</em></a></h2>\n";
    }

    void module_end( std::string const & /*module*/ )
    {
    }

    void header_start( std::string const & header )
    {
        std::cout << "    <h3><code>&lt;" << header << "&gt;</code></h3><ul>\n";
    }

    void header_end( std::string const & /*header*/ )
    {
        std::cout << "    </ul>\n";
    }

    void from_header( std::string const & header )
    {
        std::cout << "      <li>from <code>&lt;" << header << "&gt;</code></li>\n";
    }
};

static void output_module_reverse_report( std::string const & module, bool html )
{
    if( html )
    {
        module_reverse_html_actions actions;
        output_module_reverse_report( module, actions );
    }
    else
    {
        module_reverse_txt_actions actions;
        output_module_reverse_report( module, actions );
    }
}

// module_level_report

int const unknown_level = INT_MAX / 2;

struct module_level_actions
{
    virtual void heading() = 0;

    virtual void level_start( int level ) = 0;
    virtual void level_end( int level ) = 0;

    virtual void module_start( std::string const & module ) = 0;
    virtual void module_end( std::string const & module ) = 0;

    virtual void module2( std::string const & module, int level ) = 0;
};

static void output_module_level_report( module_level_actions & actions )
{
    // build module level map

    std::map< std::string, int > level_map;

    for( std::set< std::string >::iterator i = s_modules.begin(); i != s_modules.end(); ++i )
    {
        if( s_module_deps[ *i ].empty() )
        {
            level_map[ *i ] = 0;
            // std::cerr << *i << ": " << 0 << std::endl;
        }
        else
        {
            level_map[ *i ] = unknown_level;
        }
    }

    // build transitive closure to see through cycles

    std::map< std::string, std::set< std::string > > deps2 = s_module_deps;

    {
        bool done;

        do
        {
            done = true;

            for( std::map< std::string, std::set< std::string > >::iterator i = deps2.begin(); i != deps2.end(); ++i )
            {
                std::set< std::string > tmp = i->second;

                for( std::set< std::string >::iterator j = i->second.begin(); j != i->second.end(); ++j )
                {
                    std::set< std::string > tmp2 = deps2[ *j ];
                    tmp.insert( tmp2.begin(), tmp2.end() );
                }

                if( tmp.size() != i->second.size() )
                {
                    i->second = tmp;
                    done = false;
                }
            }
        }
        while( !done );
    }

    // compute acyclic levels

    for( int k = 1, n = s_modules.size(); k < n; ++k )
    {
        for( std::map< std::string, std::set< std::string > >::iterator i = s_module_deps.begin(); i != s_module_deps.end(); ++i )
        {
            // i->first depends on i->second

            if( level_map[ i->first ] >= unknown_level )
            {
                int level = 0;

                for( std::set< std::string >::iterator j = i->second.begin(); j != i->second.end(); ++j )
                {
                    level = std::max( level, level_map[ *j ] + 1 );
                }

                if( level == k )
                {
                    level_map[ i->first ] = level;
                    // std::cerr << i->first << ": " << level << std::endl;
                }
            }
        }
    }

    // min_level_map[ M ] == L means the level is unknown, but at least L
    std::map< std::string, int > min_level_map;

    // initialize min_level_map for acyclic dependencies

    for( std::map< std::string, int >::iterator i = level_map.begin(); i != level_map.end(); ++i )
    {
        if( i->second < unknown_level )
        {
            min_level_map[ i->first ] = i->second;
        }
    }

    // compute levels for cyclic modules

    for( int k = 1, n = s_modules.size(); k < n; ++k )
    {
        for( std::map< std::string, std::set< std::string > >::iterator i = s_module_deps.begin(); i != s_module_deps.end(); ++i )
        {
            if( level_map[ i->first ] >= unknown_level )
            {
                int level = 0;

                for( std::set< std::string >::iterator j = i->second.begin(); j != i->second.end(); ++j )
                {
                    int jl = level_map[ *j ];

                    if( jl < unknown_level )
                    {
                        level = std::max( level, jl + 1 );
                    }
                    else
                    {
                        int ml = min_level_map[ *j ];

                        if( deps2[ *j ].count( i->first ) == 0 )
                        {
                            // *j does not depend on i->first, so
                            // the level of i->first is at least
                            // 1 + the minimum level of *j

                            ++ml;
                        }

                        level = std::max( level, ml );
                    }
                }

                min_level_map[ i->first ] = level;
            }
        }
    }

    // reverse level map

    std::map< int, std::set< std::string > > reverse_level_map;

    for( std::map< std::string, int >::iterator i = level_map.begin(); i != level_map.end(); ++i )
    {
        int level = i->second;

        if( level >= unknown_level )
        {
            int min_level = min_level_map[ i->first ];

            if( min_level != 0 )
            {
                level = min_level;
            }
        }

        reverse_level_map[ level ].insert( i->first );
    }

    // output report

    actions.heading();

    for( std::map< int, std::set< std::string > >::iterator i = reverse_level_map.begin(); i != reverse_level_map.end(); ++i )
    {
        actions.level_start( i->first );

        for( std::set< std::string >::iterator j = i->second.begin(); j != i->second.end(); ++j )
        {
            actions.module_start( *j );

            std::set< std::string > mdeps = s_module_deps[ *j ];

            for( std::set< std::string >::iterator k = mdeps.begin(); k != mdeps.end(); ++k )
            {
                int level = level_map[ *k ];

                if( level >= unknown_level )
                {
                    int min_level = min_level_map[ *k ];

                    if( min_level != 0 )
                    {
                        level = min_level;
                    }
                }

                actions.module2( *k, level );
            }

            actions.module_end( *j );
        }

        actions.level_end( i->first );
    }
}

struct module_level_txt_actions: public module_level_actions
{
    int level_;

    void heading()
    {
        std::cout << "Module Levels:\n\n";
    }

    void level_start( int level )
    {
        if( level >= unknown_level )
        {
            std::cout << "Level (undetermined):\n";
        }
        else
        {
            std::cout << "Level " << level << ":\n";
        }

        level_ = level;
    }

    void level_end( int /*level*/ )
    {
        std::cout << "\n";
    }

    void module_start( std::string const & module )
    {
        std::cout << "    " << module;

        if( level_ > 0 )
        {
            std::cout << " ->";
        }
    }

    void module_end( std::string const & /*module*/ )
    {
        std::cout << "\n";
    }

    void module2( std::string const & module, int level )
    {
        std::cout << " " << module << "(";

        if( level >= unknown_level )
        {
            std::cout << "-";
        }
        else
        {
            std::cout << level;
        }

        std::cout << ")";
    }
};

struct module_level_html_actions: public module_level_actions
{
    int level_;

    void heading()
    {
        std::cout << "<h1>Module Levels</h1>\n";
    }

    void level_start( int level )
    {
        std::cout << "  <h2>Level ";

        if( level >= unknown_level )
        {
            std::cout << "<em>undetermined</em>";
        }
        else
        {
            std::cout << level;
        }

        std::cout << "</h2><ul>\n";

        level_ = level;
    }

    void level_end( int /*level*/ )
    {
        std::cout << "  </ul>\n";
    }

    void module_start( std::string const & module )
    {
        std::cout << "    <li><a href =\"" << module << ".html\">" << module << "</a><small>";

        if( level_ > 0 )
        {
            std::cout << "<br />&#8674;";
        }
    }

    void module_end( std::string const & /*module*/ )
    {
        std::cout << "</small></li>\n";
    }

    void module2( std::string const & module, int level )
    {
        std::cout << " ";

        bool important = level < unknown_level && level > 1 && level >= level_ - 1;

        if( important )
        {
            std::cout << "<strong>";
        }

        std::cout << module;

        if( level < unknown_level )
        {
            std::cout << "<sup>" << level << "</sup>";
        }

        if( important )
        {
            std::cout << "</strong>";
        }
    }
};

static void output_module_level_report( bool html )
{
    if( html )
    {
        module_level_html_actions actions;
        output_module_level_report( actions );
    }
    else
    {
        module_level_txt_actions actions;
        output_module_level_report( actions );
    }
}

// module_overview_report

struct module_overview_actions
{
    virtual void heading() = 0;

    virtual void module_start( std::string const & module ) = 0;
    virtual void module_end( std::string const & module ) = 0;

    virtual void module2( std::string const & module ) = 0;
};

static void output_module_overview_report( module_overview_actions & actions )
{
    actions.heading();

    for( std::set< std::string >::iterator i = s_modules.begin(); i != s_modules.end(); ++i )
    {
        actions.module_start( *i );

        std::set< std::string > const mdeps = s_module_deps[ *i ];

        for( std::set< std::string >::const_iterator j = mdeps.begin(); j != mdeps.end(); ++j )
        {
            actions.module2( *j );
        }

        actions.module_end( *i );
    }
}

struct module_overview_txt_actions: public module_overview_actions
{
    bool deps_;

    void heading()
    {
        std::cout << "Module Overview:\n\n";
    }

    void module_start( std::string const & module )
    {
        std::cout << module;
        deps_ = false;
    }

    void module_end( std::string const & /*module*/ )
    {
        std::cout << "\n";
    }

    void module2( std::string const & module )
    {
        if( !deps_ )
        {
            std::cout << " ->";
            deps_ = true;
        }

        std::cout << " " << module;
    }
};

struct module_overview_html_actions: public module_overview_actions
{
    bool deps_;

    void heading()
    {
        std::cout << "<h1>Module Overview</h1>\n";
    }

    void module_start( std::string const & module )
    {
        std::cout << "  <h2><a href =\"" << module << ".html\"><em>" << module << "</em></a></h2><p><small>";
        deps_ = false;
    }

    void module_end( std::string const & /*module*/ )
    {
        std::cout << "</small></p>\n";
    }

    void module2( std::string const & module )
    {
        if( !deps_ )
        {
            std::cout << "&#8674;";
            deps_ = true;
        }

        std::cout << " " << module;
    }
};

static void output_module_overview_report( bool html )
{
    if( html )
    {
        module_overview_html_actions actions;
        output_module_overview_report( actions );
    }
    else
    {
        module_overview_txt_actions actions;
        output_module_overview_report( actions );
    }
}

// list_dependencies

struct list_dependencies_actions: public module_overview_actions
{
    void heading()
    {
    }

    void module_start( std::string const & module )
    {
        std::cout << module << " ->";
    }

    void module_end( std::string const & /*module*/ )
    {
        std::cout << "\n";
    }

    void module2( std::string const & module )
    {
        if( module != "(unknown)" )
        {
            std::cout << " " << module;
        }
    }
};

static void list_dependencies()
{
    list_dependencies_actions actions;
    output_module_overview_report( actions );
}

//

static void output_html_header( std::string const & title )
{
    std::cout << "<html>\n";
    std::cout << "<head>\n";
    std::cout << "<title>" << title << "</title>\n";
    std::cout << "</head>\n";
    std::cout << "<body>\n";
}

static void output_html_footer( std::string const & footer )
{
    std::cout << "<hr />\n";
    std::cout << "<p><small>" << footer << "</small></p>\n";
    std::cout << "</body>\n";
    std::cout << "</html>\n";
}

static void enable_secondary( bool & secondary, bool track_sources )
{
    if( !secondary )
    {
        try
        {
            build_module_dependency_map( track_sources );
        }
        catch( fs::filesystem_error const & x )
        {
            std::cout << x.what() << std::endl;
        }

        secondary = true;
    }
}

static void list_modules()
{
    for( std::set< std::string >::iterator i = s_modules.begin(); i != s_modules.end(); ++i )
    {
        std::cout << *i << "\n";
    }
}

static void list_buildable()
{
    for( std::set< std::string >::iterator i = s_modules.begin(); i != s_modules.end(); ++i )
    {
        if( fs::exists( module_build_path( *i ) ) && fs::exists( module_source_path( *i ) ) )
        {
            std::cout << *i << "\n";
        }
    }
}

// module_weight_report

struct module_weight_actions
{
    virtual void heading() = 0;

    virtual void weight_start( int weight ) = 0;
    virtual void weight_end( int weight ) = 0;

    virtual void module_start( std::string const & module ) = 0;
    virtual void module_end( std::string const & module ) = 0;

    virtual void module_primary_start() = 0;
    virtual void module_primary( std::string const & module, int weight ) = 0;
    virtual void module_primary_end() = 0;

    virtual void module_secondary_start() = 0;
    virtual void module_secondary( std::string const & module, int weight ) = 0;
    virtual void module_secondary_end() = 0;
};

static void output_module_weight_report( module_weight_actions & actions )
{
    // gather secondary dependencies

    struct build_secondary_deps: public module_secondary_actions
    {
        std::map< std::string, std::set< std::string > > * pm_;

        build_secondary_deps( std::map< std::string, std::set< std::string > > * pm ): pm_( pm )
        {
        }

        std::string module_;

        void heading( std::string const & module )
        {
            module_ = module;
        }

        void module_start( std::string const & /*module*/ )
        {
        }

        void module_end( std::string const & /*module*/ )
        {
        }

        void module_adds( std::string const & module )
        {
            (*pm_)[ module_ ].insert( module );
        }
    };

    std::map< std::string, std::set< std::string > > secondary_deps;

    build_secondary_deps bsd( &secondary_deps );

    for( std::set< std::string >::const_iterator i = s_modules.begin(); i != s_modules.end(); ++i )
    {
        output_module_secondary_report( *i, bsd );
    }

    // build weight map

    std::map< int, std::set< std::string > > modules_by_weight;

    for( std::set< std::string >::const_iterator i = s_modules.begin(); i != s_modules.end(); ++i )
    {
        int w = s_module_deps[ *i ].size() + secondary_deps[ *i ].size();
        modules_by_weight[ w ].insert( *i );
    }

    // output report

    actions.heading();

    for( std::map< int, std::set< std::string > >::const_iterator i = modules_by_weight.begin(); i != modules_by_weight.end(); ++i )
    {
        actions.weight_start( i->first );

        for( std::set< std::string >::const_iterator j = i->second.begin(); j != i->second.end(); ++j )
        {
            actions.module_start( *j );

            if( !s_module_deps[ *j ].empty() )
            {
                actions.module_primary_start();

                for( std::set< std::string >::const_iterator k = s_module_deps[ *j ].begin(); k != s_module_deps[ *j ].end(); ++k )
                {
                    int w = s_module_deps[ *k ].size() + secondary_deps[ *k ].size();
                    actions.module_primary( *k, w );
                }

                actions.module_primary_end();
            }

            if( !secondary_deps[ *j ].empty() )
            {
                actions.module_secondary_start();

                for( std::set< std::string >::const_iterator k = secondary_deps[ *j ].begin(); k != secondary_deps[ *j ].end(); ++k )
                {
                    int w = s_module_deps[ *k ].size() + secondary_deps[ *k ].size();
                    actions.module_secondary( *k, w );
                }

                actions.module_secondary_end();
            }

            actions.module_end( *j );
        }

        actions.weight_end( i->first );
    }
}

struct module_weight_txt_actions: public module_weight_actions
{
    void heading()
    {
        std::cout << "Module Weights:\n\n";
    }

    void weight_start( int weight )
    {
        std::cout << "Weight " << weight << ":\n";
    }

    void weight_end( int /*weight*/ )
    {
        std::cout << "\n";
    }

    void module_start( std::string const & module )
    {
        std::cout << "    " << module;
    }

    void module_end( std::string const & /*module*/ )
    {
        std::cout << "\n";
    }

    void module_primary_start()
    {
        std::cout << " ->";
    }

    void module_primary( std::string const & module, int weight )
    {
        std::cout << " " << module << "(" << weight << ")";
    }

    void module_primary_end()
    {
    }

    void module_secondary_start()
    {
        std::cout << " ->";
    }

    void module_secondary( std::string const & module, int /*weight*/ )
    {
        std::cout << " " << module;
    }

    void module_secondary_end()
    {
    }
};

struct module_weight_html_actions: public module_weight_actions
{
    int weight_;

    void heading()
    {
        std::cout << "<h1>Module Weights</h1>\n";
    }

    void weight_start( int weight )
    {
        std::cout << "  <h2>Weight " << weight << "</h2><ul>\n";
        weight_ = weight;
    }

    void weight_end( int /*weight*/ )
    {
        std::cout << "  </ul>\n";
    }

    void module_start( std::string const & module )
    {
        std::cout << "    <li><a href =\"" << module << ".html\">" << module << "</a><small>";
    }

    void module_end( std::string const & /*module*/ )
    {
        std::cout << "</small></li>\n";
    }

    void module_primary_start()
    {
        std::cout << "<br />&#8674;";
    }

    void module_primary( std::string const & module, int weight )
    {
        std::cout << " ";

        bool heavy = weight >= 0.8 * weight_;

        if( heavy )
        {
            std::cout << "<strong>";
        }

        std::cout << module << "<sup>" << weight << "</sup>";

        if( heavy )
        {
            std::cout << "</strong>";
        }
    }

    void module_primary_end()
    {
    }

    void module_secondary_start()
    {
        std::cout << "<br /><span style=\"padding-left: 1em;\">&#8674;";
    }

    void module_secondary( std::string const & module, int /*weight*/ )
    {
        std::cout << " " << module;
    }

    void module_secondary_end()
    {
        std::cout << "</span>";
    }
};

static void output_module_weight_report( bool html )
{
    if( html )
    {
        module_weight_html_actions actions;
        output_module_weight_report( actions );
    }
    else
    {
        module_weight_txt_actions actions;
        output_module_weight_report( actions );
    }
}

// output_module_subset_report

struct module_subset_actions
{
    virtual void heading( std::string const & module ) = 0;

    virtual void module_start( std::string const & module ) = 0;
    virtual void module_end( std::string const & module ) = 0;

    virtual void from_path( std::vector<std::string> const & path ) = 0;
};

static void output_module_subset_report( std::string const & module, module_subset_actions & actions )
{
    // build header closure

    std::set<std::string> const & headers = s_module_headers[ module ];

    // header -> (header)*
    std::map< std::string, std::set<std::string> > inc2;

    // (header, header) -> path
    std::map< std::pair<std::string, std::string>, std::vector<std::string> > paths;

    for( std::set<std::string>::const_iterator i = headers.begin(); i != headers.end(); ++i )
    {
        std::set<std::string> & s = inc2[ *i ];

        s = s_header_includes[ *i ];

        for( std::set<std::string>::const_iterator j = s.begin(); j != s.end(); ++j )
        {
            std::vector<std::string> & v = paths[ std::make_pair( *i, *j ) ];

            v.resize( 0 );
            v.push_back( *i );
            v.push_back( *j );
        }
    }

    for( ;; )
    {
        bool r = false;

        for( std::map< std::string, std::set<std::string> >::iterator i = inc2.begin(); i != inc2.end(); ++i )
        {
            std::set<std::string> & s2 = i->second;

            for( std::set<std::string>::const_iterator j = s2.begin(); j != s2.end(); ++j )
            {
                std::set<std::string> const & s = s_header_includes[ *j ];

                for( std::set<std::string>::const_iterator k = s.begin(); k != s.end(); ++k )
                {
                    if( s2.count( *k ) == 0 )
                    {
                        s2.insert( *k );

                        std::vector<std::string> const & v1 = paths[ std::make_pair( i->first, *j ) ];
                        std::vector<std::string> & v2 = paths[ std::make_pair( i->first, *k ) ];

                        v2 = v1;
                        v2.push_back( *k );

                        r = true;
                    }
                }
            }
        }

        if( !r ) break;
    }

    // module -> header -> path [header -> header -> header]
    std::map< std::string, std::map< std::string, std::vector<std::string> > > subset;

    for( std::set<std::string>::const_iterator i = headers.begin(); i != headers.end(); ++i )
    {
        std::set<std::string> const & s = inc2[ *i ];

        for( std::set<std::string>::const_iterator j = s.begin(); j != s.end(); ++j )
        {
            std::string const & m = s_header_map[ *j ];

            if( m.empty() ) continue;

            std::vector<std::string> const & path = paths[ std::make_pair( *i, *j ) ];

            if( subset.count( m ) == 0 || subset[ m ].count( *i ) == 0 || subset[ m ][ *i ].size() > path.size() )
            {
                subset[ m ][ *i ] = path;
            }
        }
    }

    actions.heading( module );

    for( std::map< std::string, std::map< std::string, std::vector<std::string> > >::const_iterator i = subset.begin(); i != subset.end(); ++i )
    {
        if( i->first == module ) continue;

        actions.module_start( i->first );

        int k = 0;

        for( std::map< std::string, std::vector<std::string> >::const_iterator j = i->second.begin(); j != i->second.end() && k < 4; ++j, ++k )
        {
            actions.from_path( j->second );
        }

        actions.module_end( i->first );
    }
}

struct module_subset_txt_actions: public module_subset_actions
{
    void heading( std::string const & module )
    {
        std::cout << "Subset dependencies for " << module << ":\n\n";
    }

    void module_start( std::string const & module )
    {
        std::cout << module << ":\n";
    }

    void module_end( std::string const & /*module*/ )
    {
        std::cout << "\n";
    }

    void from_path( std::vector<std::string> const & path )
    {
        for( std::vector<std::string>::const_iterator i = path.begin(); i != path.end(); ++i )
        {
            if( i == path.begin() )
            {
                std::cout << "  ";
            }
            else
            {
                std::cout << " -> ";
            }

            std::cout << *i;
        }

        std::cout << "\n";
    }
};

struct module_subset_html_actions: public module_subset_actions
{
    void heading( std::string const & module )
    {
        std::cout << "\n\n<h1 id=\"subset-dependencies\">Subset dependencies for <em>" << module << "</em></h1>\n";
    }

    void module_start( std::string const & module )
    {
        std::cout << "  <h2 id=\"subset-" << module << "\"><a href=\"" << module << ".html\"><em>" << module << "</em></a></h2><ul>\n";
    }

    void module_end( std::string const & /*module*/ )
    {
        std::cout << "</ul>\n";
    }

    void from_path( std::vector<std::string> const & path )
    {
        std::cout << "    <li>";

        for( std::vector<std::string>::const_iterator i = path.begin(); i != path.end(); ++i )
        {
            if( i != path.begin() )
            {
                std::cout << " &#8674; ";
            }

            std::cout << "<code>" << *i << "</code>";
        }

        std::cout << "</li>\n";
    }
};

static void output_module_subset_report( std::string const & module, bool html )
{
    if( html )
    {
        module_subset_html_actions actions;
        output_module_subset_report( module, actions );
    }
    else
    {
        module_subset_txt_actions actions;
        output_module_subset_report( module, actions );
    }
}

int main( int argc, char const* argv[] )
{
    if( argc < 2 )
    {
        std::cout <<

            "Usage:\n"
            "\n"
            "    boostdep --list-modules\n"
            "    boostdep --list-buildable\n"
            "    boostdep [--track-sources] --list-dependencies\n"
            "\n"
            "    boostdep [options] --module-overview\n"
            "    boostdep [options] --module-levels\n"
            "    boostdep [options] --module-weights\n"
            "\n"
            "    boostdep [options] [--primary] <module>\n"
            "    boostdep [options] --secondary <module>\n"
            "    boostdep [options] --reverse <module>\n"
            "    boostdep [options] --subset <module>\n"
            "    boostdep [options] [--header] <header>\n"
            "\n"
            "    [options]: [--track-sources] [--title <title>] [--footer <footer>] [--html]\n";

        return -1;
    }

    try
    {
        build_header_map();
    }
    catch( fs::filesystem_error const & x )
    {
        std::cout << x.what() << std::endl;
    }

    bool html = false;
    bool secondary = false;
    bool track_sources = false;

    std::string title = "Boost Dependency Report";
    std::string footer;

    for( int i = 1; i < argc; ++i )
    {
        std::string option = argv[ i ];

        if( option == "--list-modules" )
        {
            list_modules();
        }
        else if( option == "--list-buildable" )
        {
            list_buildable();
        }
        else if( option == "--title" )
        {
            if( i + 1 < argc )
            {
                title = argv[ ++i ];
            }
        }
        else if( option == "--footer" )
        {
            if( i + 1 < argc )
            {
                footer = argv[ ++i ];
            }
        }
        else if( option == "--html" )
        {
            if( !html )
            {
                html = true;
                output_html_header( title );
            }
        }
        else if( option == "--track-sources" )
        {
            track_sources = true;
        }
        else if( option == "--primary" )
        {
            if( i + 1 < argc )
            {
                output_module_primary_report( argv[ ++i ], html, track_sources );
            }
        }
        else if( option == "--secondary" )
        {
            if( i + 1 < argc )
            {
                enable_secondary( secondary, track_sources );
                output_module_secondary_report( argv[ ++i ], html );
            }
        }
        else if( option == "--reverse" )
        {
            if( i + 1 < argc )
            {
                enable_secondary( secondary, track_sources );
                output_module_reverse_report( argv[ ++i ], html );
            }
        }
        else if( option == "--header" )
        {
            if( i + 1 < argc )
            {
                enable_secondary( secondary, track_sources );
                output_header_report( argv[ ++i ], html );
            }
        }
        else if( option == "--subset" )
        {
            if( i + 1 < argc )
            {
                enable_secondary( secondary, track_sources );
                output_module_subset_report( argv[ ++i ], html );
            }
        }
        else if( option == "--module-levels" )
        {
            enable_secondary( secondary, track_sources );
            output_module_level_report( html );
        }
        else if( option == "--module-overview" )
        {
            enable_secondary( secondary, track_sources );
            output_module_overview_report( html );
        }
        else if( option == "--module-weights" )
        {
            enable_secondary( secondary, track_sources );
            output_module_weight_report( html );
        }
        else if( option == "--list-dependencies" )
        {
            enable_secondary( secondary, track_sources );
            list_dependencies();
        }
        else if( s_modules.count( option ) )
        {
            output_module_primary_report( option, html, track_sources );
        }
        else if( s_header_map.count( option ) )
        {
            enable_secondary( secondary, track_sources );
            output_header_report( option, html );
        }
        else
        {
            std::cerr << "'" << option << "': not an option, module or header.\n";
        }
    }

    if( html )
    {
        output_html_footer( footer );
    }
}
