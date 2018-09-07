#include "cxx_function.hpp"
using namespace cxx_function;


void f(function<void()>) {}
void f(function<void(int)>) {}

int main() {
  f([]{});
  f([](int){});
}

