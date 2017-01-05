BOOST_ROOT = $(DEVROOT)/boost

cflags    = -O3 -Iinclude -I$(BOOST_ROOT) -Wno-unused-variable -Wno-uninitialized
ldflags   = -lboost_timer -lboost_system -lboost_chrono

target_1  = convert-test-callable           test/callable.cpp
target_2  = convert-test-is-converter       test/is_converter.cpp
target_3  = convert-test-fallbacks          test/fallbacks.cpp
target_4  = convert-test-spirit-converter   test/spirit_converter.cpp
target_5  = convert-test-stream-converter   test/stream_converter.cpp
target_6  = convert-test-lcast-converter    test/lcast_converter.cpp
target_7  = convert-test-printf-converter   test/printf_converter.cpp
target_8  = convert-test-strtol-converter   test/strtol_converter.cpp
target_9  = convert-test-encryption         test/encryption.cpp
target_10 = convert-test-user-type          test/user_type.cpp
target_11 = convert-test-str-to-int         test/str_to_int.cpp
target_12 = convert-test-sfinae             test/sfinae.cpp
target_13 = convert-test-has-member         test/has_member.cpp
target_14 = convert-test-performance        test/performance.cpp
target_15 = convert-test-performance-spirit test/performance_spirit.cpp

target_21 = convert-example-algorithms         example/algorithms.cpp
target_22 = convert-example-default_converter  example/default_converter.cpp
target_23 = convert-example-getting_serious    example/getting_serious.cpp
target_24 = convert-example-getting_started    example/getting_started.cpp
target_25 = convert-example-lexical_cast       example/lexical_cast.cpp
target_26 = convert-example-stream             example/stream.cpp

include $(DEVMAKE)/makefile
