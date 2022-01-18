// Boost.Geometry (aka GGL, Generic Geometry Library)
// Tool reporting Implementation Status in QBK format

// Copyright (c) 2011-2014 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2011-2014 Bruno Lalande, Paris, France.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <stdlib.h>

#include <boost/timer.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>

static const int point = 0;
static const int segment = 1;
static const int box = 2;
static const int linestring = 3;
static const int ring = 4;
static const int polygon = 5;
static const int multi_point = 6;
static const int multi_linestring = 7;
static const int multi_polygon = 8;
static const int variant = 9;
static const int geometry_count = 10;

struct compile_bjam
{
    static inline bool apply(std::string const& id)
    {
        std::ostringstream command;
        // For debugging:
        command << "b2 -a tmp > tmp/t_" << id << ".out";
        //command << "b2 -a tmp > tmp/t.out";
        int failed = system(command.str().c_str());

        {
            // For debugging: save t.cpp
            std::ostringstream c2;
            c2 << "cp tmp/t.cpp tmp/t_" << id << ".cpp";
            system(c2.str().c_str());
        }
        return failed == 0;
    }
};


struct compile_clang
{
    bool first;

    compile_clang()
        : first(true)
    {}

    inline bool apply(std::string const& id)
    {
        if (first)
        {
           // Generate the pre-compiled header
           system("clang -x c++-header -I . -I ../../../../../../.. implementation_status.hpp");
           first = false;
        }

        std::ostringstream command;
        // We compile only, not even link
        command << "clang -include implementation_status.hpp -I . -I ../../../../../../.. -c tmp/t.cpp > tmp/t_" << id << ".out 2>&1";
        int failed = system(command.str().c_str());

        {
            // For debugging: save t.cpp
            std::ostringstream c2;
            c2 << "cp tmp/t.cpp tmp/t_" << id << ".cpp";
            system(c2.str().c_str());
        }
        return failed == 0;
    }
};

struct compile_msvc
{
    bool first;
    int count;

    compile_msvc()
        : first(true)
        , count(0)
    {}

    inline bool apply(std::string const& id)
    {
        std::ostringstream command;
        command << "cl /nologo -I. -I/_svn/boost/trunk /EHsc /Y";
        if (first)
        {
            std::cout << " (creating PCH)";
            command << "c";
            first = false;
        }
        else
        {
            command <<  "u";
        }

        command << "implementation_status.hpp tmp/t.cpp > tmp/t" //.out";
            // For debugging:
            << id << ".out";

        int failed = system(command.str().c_str());
        return failed == 0;
    }
};

struct algorithm
{
    std::string name;
    int arity;

    explicit algorithm(std::string const& n, int a = 1)
        : name(n)
        , arity(a)
    {}
};


inline std::string bool_string(bool v)
{
    return v ? "true" : "false";
}

inline std::string typedef_string(int type, bool clockwise, bool open)
{
    std::ostringstream out;
    switch(type)
    {
        case point : return "P";
        case linestring : return "bg::model::linestring<P>";
        case box : return "bg::model::box<P>";
        case segment : return "bg::model::segment<P>";
        case ring :
            out << "bg::model::ring<P, "
                << bool_string(clockwise) << ", " << bool_string(open) << ">";
            break;
        case variant :
        case polygon :
            out << "bg::model::polygon<P, "
                << bool_string(clockwise) << ", " << bool_string(open) << ">";
            break;
        case multi_point : return "bg::model::multi_point<P>";
        case multi_linestring :
            out << "bg::model::multi_linestring<bg::model::linestring<P> >";
            break;
        case multi_polygon :
            out << "bg::model::multi_polygon<bg::model::polygon<P, "
                << bool_string(clockwise) << ", " << bool_string(open) << "> >";
            break;
    }
    return out.str();
}

inline std::string wkt_string(int type)
{
    switch(type)
    {
        case point : return "POINT(1 1)";
        case linestring : return "LINESTRING(1 1,2 2)";
        case segment : return "LINESTRING(1 1,2 2)";
        case box : return "POLYGON((1 1,2 2))";
        case polygon :
        case variant :
        case ring :
            return "POLYGON((0 0,0 1,1 1,0 0))";
        case multi_point : return "MULTIPOINT((1 1),(2 2))";
        case multi_linestring : return "MULTILINESTRING((1 1,2 2))";
        case multi_polygon : return "MULTIPOLYGON(((0 0,0 1,1 1,0 0)))";
    }
    return "";
}

inline std::string geometry_string(int type)
{
    switch(type)
    {
        case point : return "Point";
        case linestring : return "Linestring";
        case box : return "Box";
        case polygon : return "Polygon";
        case ring : return "Ring";
        case segment : return "Segment";
        case multi_point : return "MultiPoint";
        case multi_linestring : return "MultiLinestring";
        case multi_polygon : return "MultiPolygon";
        case variant : return "Variant";
    }
    return "";
}

template <typename CompilePolicy>
int report_library(CompilePolicy& compile_policy,
                   int type, algorithm const& algo, bool clockwise,
                   bool open, int dimensions, std::string const& cs,
                   int type2 = -1)
{
    std::string lit;
    {
        std::ostringstream out;
        out << geometry_string(type);
        if (type2 != -1)
        {
            out << "_" << geometry_string(type2);
        }
        out
            << "_" << algo.name
            << "_" << bool_string(clockwise)
            << "_" << bool_string(open)
            << "_" << boost::replace_all_copy
                        (
                            boost::replace_all_copy
                                (
                                    boost::replace_all_copy(cs, "bg::", "")
                                , "<", "_"
                                )
                            , ">", "_"
                        );
        lit = out.str();
    }

    std::cout << lit;

    {
        std::ofstream out("tmp/t.cpp");

        std::string name = "geometry";

        if (type == variant)
        {
            name = "source";
        }

        out << "#include <implementation_status.hpp>" << std::endl;

        if (type == variant)
        {
            out << "#include <boost/variant/variant.hpp>" << std::endl;
        }

        out
            << "template <typename P>" << std::endl
            << "inline void test()" << std::endl
            << "{" << std::endl
            << "  namespace bg = boost::geometry;" << std::endl
            << "  " << typedef_string(type, clockwise, open) << " " << name << ";" << std::endl
            << "  bg::read_wkt(\"" << wkt_string(type) << "\", " << name << ");" << std::endl;

        if (type == variant)
        {
            out
                << "  typedef " << typedef_string(polygon, clockwise, open) << " type1;" << std::endl
                << "  typedef " << typedef_string(box, clockwise, open) << " type2;" << std::endl
                << "  boost::variant<type1, type2> geometry;" << std::endl
                << "  geometry = source;"
                << std::endl;
        }

        if (algo.arity > 1)
        {
            out
                << "  " << typedef_string(type2, clockwise, open) << " geometry2;" << std::endl
                << "  bg::read_wkt(\"" << wkt_string(type2) << "\", geometry2);" << std::endl;
        }

        if (algo.name == std::string("centroid"))
        {
            out << "  P point;";
            out << "  bg::" << algo.name << "(geometry, point);" << std::endl;
        }
        else if (algo.name == std::string("envelope"))
        {
            out << "  bg::model::box<P> box;";
            out << "  bg::" << algo.name << "(geometry, box);" << std::endl;
        }
        else
        {
            switch(algo.arity)
            {
                case 1 :
                    out << "  bg::" << algo.name << "(geometry);" << std::endl;
                    break;
                case 2 :
                    // For cases as point-in-polygon, take first geometry 2 (point), then geometry (polygon) such that
                    // it is listed as column:point in row:polygon
                    out << "  bg::" << algo.name << "(geometry2, geometry);" << std::endl;
                    break;
            }
        }

        out
            << "}" << std::endl
            << std::endl
            ;

        out
            << "int main()" << std::endl
            << "{" << std::endl
            << "  namespace bg = boost::geometry;" << std::endl
            << "  test<bg::model::point< double, " << dimensions << ", bg::cs::" << cs << " > >();" << std::endl
            << "  return 0;" << std::endl
            << "}" << std::endl
            << std::endl
            ;
    }

    bool result = compile_policy.apply(lit);
    if (! result)
    {
        std::cout << " ERROR";
    }
    std::cout << std::endl;
    return result;
}


template <typename CompilePolicy>
std::vector<int> report(CompilePolicy& compile_policy,
                        int type, algorithm const& algo, bool clockwise,
                        bool open, int dimensions, std::string const& cs)
{
    std::vector<int> result;

    switch(algo.arity)
    {
        case 1 :
            result.push_back(report_library(compile_policy, type, algo, clockwise, open, dimensions, cs));
            break;
        case 2 :
            for (int type2 = point; type2 < geometry_count; ++type2)
            {
                result.push_back(report_library(compile_policy, type, algo, clockwise, open, dimensions, cs, type2));
            }
            break;
    }

    return result;
}


struct cs
{
    std::string name;

    cs(std::string const& n)
        : name(n)
    {}
};


int main(int , char** )
{
#if defined(_MSC_VER)
    compile_msvc compile_policy;
#else
    //compile_bjam compile_policy;
    compile_clang compile_policy;
#endif

    typedef std::vector<algorithm> v_a_type;
    v_a_type algorithms;
    algorithms.push_back(algorithm("area"));
    algorithms.push_back(algorithm("clear"));
    algorithms.push_back(algorithm("correct"));
    algorithms.push_back(algorithm("centroid")); // NOTE: current doc contains 2D / 3D
    algorithms.push_back(algorithm("envelope"));
    algorithms.push_back(algorithm("length"));
    algorithms.push_back(algorithm("is_simple"));
    algorithms.push_back(algorithm("is_valid"));
    algorithms.push_back(algorithm("num_points"));
    algorithms.push_back(algorithm("perimeter"));

    algorithms.push_back(algorithm("covered_by", 2));
    algorithms.push_back(algorithm("distance", 2));
    algorithms.push_back(algorithm("crosses", 2));
    algorithms.push_back(algorithm("disjoint", 2));
    algorithms.push_back(algorithm("equals", 2));
    algorithms.push_back(algorithm("intersects", 2));
    algorithms.push_back(algorithm("overlaps", 2));
    algorithms.push_back(algorithm("within", 2));

    typedef std::vector<cs> cs_type;
    cs_type css;
    css.push_back(cs("cartesian"));
    // css.push_back(cs("spherical<bg::degree>"));
    // css.push_back(cs("spherical<bg::radian>"));


    boost::timer timer;

    for (v_a_type::const_iterator it = algorithms.begin(); it != algorithms.end(); ++it)
    {
/*([heading Behavior]
[table
[[Case] [Behavior] ]
[[__2dim__][All combinations of: box, ring, polygon, multi_polygon]]
[[__other__][__nyiversion__]]
[[__sph__][__nyiversion__]]
[[Three dimensional][__nyiversion__]]
]*/

        std::ostringstream name;
        name << "../../../../reference/status/" << it->name << "_status.qbk";

        std::ofstream out(name.str().c_str());
        out << "[heading Supported geometries]" << std::endl;

        cs_type::const_iterator cit = css.begin();

        {
            // Construct the table

            std::vector<std::vector<int> > table;

            for (int type = point; type < geometry_count; type++)
            {
                table.push_back(report(compile_policy, type, *it, true, true, 2, cit->name));
            }


#if SURPRESS
            // Detect red rows/columns

            std::vector<int> lines_status(table.size(), false);
            std::vector<int> columns_status(table[0].size(), false);

            for (unsigned int i = 0; i != table.size(); ++i)
            {
                for (unsigned int j = 0; j != table[i].size(); ++j)
                {
                    lines_status[i] |= table[i][j];
                    columns_status[j] |= table[i][j];
                }
            }
#endif


            // Display the table

            out << "[table" << std::endl << "[";

            if (it->arity > 1)
            {
                out << "[ ]";
                for (int type = point; type < geometry_count; type++)
                {
#if SURPRESS
                    if (!columns_status[type]) continue;
#endif
                    out << "[" << geometry_string(type) << "]";
                }
            }
            else
            {
                out << "[Geometry][Status]";
            }

            out << "]" << std::endl;

            for (unsigned int i = 0; i != table.size(); ++i)
            {
#if SURPRESS
                if (!lines_status[i]) continue;
#endif
                out << "[";
                out << "[" << geometry_string(i) << "]";
                for (unsigned int j = 0; j != table[i].size(); ++j)
                {
#if SURPRESS
                    if (!columns_status[j]) continue;
#endif
                    out << "[ [$img/" << (table[i][j] ? "ok" : "nyi") << ".png] ]";
                }
                out << "]" << std::endl;
            }

            out << "]" << std::endl;
        }
    }

    std::cout << "TIME: " << timer.elapsed() << std::endl;

    return 0;
}
