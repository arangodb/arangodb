// Copyright (c) 2015 Klemens D. Morgenstern
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/process.hpp>
#include <boost/program_options.hpp>

#include <vector>
#include <string>

#include <iostream>
#include <cstdint>

#include <fstream>

#include <chrono>


int main(int argc, char *argv[])
{
    using namespace std;
    using namespace boost::program_options;
    using namespace boost::process;

    bool launch_detached = false;
    bool launch_attached = false;

    options_description desc;
    desc.add_options()
         ("launch-detached", bool_switch(&launch_detached))
         ("launch-attached", bool_switch(&launch_attached))
         ;

    variables_map vm;
    command_line_parser parser(argc, argv);
    store(parser.options(desc).allow_unregistered().run(), vm);
    notify(vm);

    child c1;
    child c2;

    std::error_code ec;

    if (launch_attached)
    {
        c1 =     child(argv[0], ec,  std_out > null, std_err > null, std_in < null);
        if (ec)
        {
            cout << -1 << endl;
            cerr << ec.message() << endl;
            return 1;
        }

        cout << c1.id() << endl;
    }
    else
        cout << -1 << endl;

    if (launch_detached)
    {
        group g;

        c2 = child(argv[0], ec, g, std_out > null, std_err > null, std_in < null);
        if (ec)
        {
            cout << -1 << endl;
            cerr << ec.message() << endl;
            return 1;
        }
        else
            cout << c2.id() << endl;

        g.detach();
    }
    else
        cout << -1 << endl;


    this_thread::sleep_for(chrono::seconds(10));


    return 0;
}
