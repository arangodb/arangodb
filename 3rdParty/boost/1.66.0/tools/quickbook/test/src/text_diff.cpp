//
//  Copyright (c) 2005 João Abecasis
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#include <fstream>
#include <iostream>
#include <iterator>
#include <cstring>
#include <vector>

#include <boost/spirit/include/classic_scanner.hpp>
#include <boost/spirit/include/classic_primitives.hpp>

namespace spirit = boost::spirit::classic;

typedef std::istream_iterator<char, char> iterator;
typedef spirit::scanner<iterator> scanner;

int main(int argc, char * argv[])
{
    std::vector<char*> args;
    bool usage_error = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strncmp(argv[i], "--", 2) == 0) {
            if (strcmp(argv[i], "--strict") == 0) {
                // Ignore --strict because the build file accidentally
                // uses it. Why yes, this is a horrible hack.
            } else {
                std::cerr << "ERROR: Invalid flag: " << argv[i] << std::endl;
                usage_error = true;
            }
        } else {
            args.push_back(argv[i]);
        }
    }

    if (!usage_error && args.size() != 2)
    {
        std::cerr << "ERROR: Wrong number of arguments." << std::endl;
        usage_error = true;
    }

    if (usage_error) {
        std::cout << "Usage:\n\t" << argv[0] << " file1 file2" << std::endl;
        return 1;
    }

    std::ifstream
        file1(args[0], std::ios_base::binary | std::ios_base::in),
        file2(args[1], std::ios_base::binary | std::ios_base::in);

    if (!file1 || !file2)
    {
        std::cerr << "ERROR: Unable to open one or both files." << std::endl;
        return 2;
    }

    file1.unsetf(std::ios_base::skipws);
    file2.unsetf(std::ios_base::skipws);

    iterator
        iter_file1(file1),
        iter_file2(file2);

    scanner
        scan1(iter_file1, iterator()),
        scan2(iter_file2, iterator());

    std::size_t line = 1, column = 1;

    while (!scan1.at_end() && !scan2.at_end())
    {
        if (spirit::eol_p.parse(scan1))
        {
            if (!spirit::eol_p.parse(scan2))
            {
                std::cout << "Files differ at line " << line << ", column " <<
                    column << '.' << std::endl;
                return 3;
            }

            ++line, column = 1;
            continue;
        }

        if (*scan1 != *scan2)
        {
            std::cout << "Files differ at line " << line << ", column " <<
                column << '.' << std::endl;
            return 4;
        }

        ++scan1, ++scan2, ++column;
    }

    if (scan1.at_end() != scan2.at_end())
    {
        std::cout << "Files differ in length." << std::endl;
        return 5;
    }
}
