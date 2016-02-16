#include <type_traits>
#include <string>

template<class T> class A {
public: 
	typedef typename std::conditional<false, const std::string, std::string>::type StringType;
	A() : s(""), t(0) {}
	virtual ~A () {}
private: 
    StringType s;
	T t;
};

int main() {
	A<float> a;
	return 0;
}
