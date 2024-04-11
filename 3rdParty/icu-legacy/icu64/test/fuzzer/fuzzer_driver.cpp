// © 2019 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include <fstream>
#include <iostream>
#include <sstream>
#include <stddef.h>
#include <stdint.h>
#include <string>

#include "cmemory.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int main(int argc, char* argv[])
{
    bool show_warning = true;
    bool show_error = true;
#if UPRV_HAS_FEATURE(address_sanitizer)
    show_warning = false;
#endif
#if UPRV_HAS_FEATURE(memory_sanitizer)
    show_warning = false;
#endif
    if (argc > 2 && strcmp(argv[2], "-q") == 0) {
        show_warning = false;
        show_error = false;
    }
    if (show_warning) {
        std::cerr << "WARNING: This binary work only under build configure with" << std::endl
                  << " CFLAGS=\"-fsanitize=$SANITIZE\""
                  << " CXXFLAGS=\"-fsanitize=$SANITIZE\""
                  << " ./runConfigureICU ... " << std::endl
                  << "  where $SANITIZE is 'address' or 'memory'" << std::endl
                  << "Please run the above step and make tests to rebuild" << std::endl;
        // Do not return -1 here so we will pass the unit test.
    }
    if (argc < 2) {
        if (show_error) {
            std::cerr << "Usage: " << argv[0] << "  testcasefile [-q]" << std::endl
                      << "  -q : quiet while error" << std::endl;
        }
        return -1;
    }
    const char *path = argv[1];
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        if (show_error) {
            std::cerr << "Cannot open testcase file " << path << std::endl;
        }
        return -1;
    }
    std::ostringstream ostrm;
    ostrm << file.rdbuf();
    LLVMFuzzerTestOneInput((const uint8_t *) ostrm.str().c_str(), ostrm.str().size());

    return 0;
}
