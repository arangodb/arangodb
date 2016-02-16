#include <algorithm>
#include <regex>

int parse_line(std::string const& line)
{
	std::string tmp;
	if(std::regex_search(line, std::regex("(\\s)+(-)?(\\d)+//(-)?(\\d)+(\\s)+"))) {
		tmp = std::regex_replace(line, std::regex("(-)?(\\d)+//(-)?(\\d)+"), std::string("V"));
	} else if(std::regex_search(line, std::regex("(\\s)+(-)?(\\d)+/(-)?(\\d)+(\\s)+"))) {
		tmp = std::regex_replace(line, std::regex("(-)?(\\d)+/(-)?(\\d)+"), std::string("V"));
	} else if(std::regex_search(line, std::regex("(\\s)+(-)?(\\d)+/(-)?(\\d)+/(-)?(\\d)+(\\s)+"))) {
		tmp = std::regex_replace(line, std::regex("(-)?(\\d)+/(-)?(\\d)+/(-)?(\\d)+"), std::string("V"));
	} else {
		tmp = std::regex_replace(line, std::regex("(-)?(\\d)+"), std::string("V"));
	}
	return static_cast<int>(std::count(tmp.begin(), tmp.end(), 'V'));
}

int main()
{
	bool test = (parse_line("f 7/7/7 -3/3/-3 2/-2/2") == 3) &&
				(parse_line("f 7//7 3//-3 -2//2") == 3) &&
				(parse_line("f 7/7 3/-3 -2/2") == 3) &&
				(parse_line("f 7 3 -2") == 3);
	return test ? 0 : 1;
}
