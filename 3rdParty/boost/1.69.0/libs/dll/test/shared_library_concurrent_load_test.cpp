// Copyright 2015 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// For more information, see http://www.boost.org

#ifdef BOOST_TRAVISCI_BUILD

int main() {
    return 0;
}

#else // #ifdef BOOST_TRAVISCI_BUILD

#include "../example/b2_workarounds.hpp"
#include <boost/dll.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/barrier.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/bind.hpp>
#include <cctype>
#include <vector>

typedef std::vector<boost::filesystem::path> paths_t;
const std::size_t thread_count = 4;
boost::barrier b(thread_count);


// Disgusting workarounds for b2 on Windows platform
inline paths_t generate_paths(int argc, char* argv[]) {
    paths_t ret;
    ret.reserve(argc - 1);

    for (int i = 1; i < argc; ++i) {
        boost::filesystem::path p = argv[i];
        if (b2_workarounds::is_shared_library(p)) {
            ret.push_back(p);
        }
    }

    return ret;
}

inline void load_unload(const paths_t& paths, std::size_t count) {
    for (std::size_t j = 0; j < count; j += 2) {
        for (std::size_t i = 0; i < paths.size(); ++i) {
            boost::dll::shared_library lib(paths[i]);
            BOOST_TEST(lib);
        }
        for (std::size_t i = 0; i < paths.size(); ++i) {
            boost::dll::shared_library lib(paths[i]);
            BOOST_TEST(lib.location() != "");
        }

        // Waiting for all threads to unload shared libraries
        b.wait();
    }
}


int main(int argc, char* argv[]) {
    BOOST_TEST(argc >= 3);
    paths_t paths = generate_paths(argc, argv);
    BOOST_TEST(!paths.empty());

    std::cout << "Libraries:\n\t";
    std::copy(paths.begin(), paths.end(), std::ostream_iterator<boost::filesystem::path>(std::cout, ", "));
    std::cout << std::endl;

    boost::thread_group threads;
    for (std::size_t i = 0; i < thread_count; ++i) {
        threads.create_thread(boost::bind(load_unload, paths, 1000));
    }
    threads.join_all();

    return boost::report_errors();
}

#endif // #ifdef BOOST_TRAVISCI_BUILD
