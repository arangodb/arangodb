#include <string>
#include <iostream>

int main() {
  std::string my_string{"hello"};
  my_string.append("abc");
  std::cout << my_string << std::endl;
}
