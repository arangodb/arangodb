
add_executable(futures-test main.cpp static-tests.cpp test-helper.cpp test-helper.h sequencer-test.cpp exception-tests.cpp inline-value-tests.cpp user-defined.cpp bind-test.cpp swap-test.cpp call-test.cpp call-generic-test.cpp await-test.cpp)
target_compile_options(futures-test PRIVATE "-fsanitize=address")
target_link_libraries(futures-test PRIVATE asan)
target_link_libraries(futures-test PRIVATE gtest)

target_link_libraries(futures-test PRIVATE mellon)

enable_testing()
add_test(NAME future-test COMMAND futures-test)
