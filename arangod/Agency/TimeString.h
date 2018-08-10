#include <chrono>

inline std::string timepointToString(std::chrono::system_clock::time_point const& t) {
  time_t tt = std::chrono::system_clock::to_time_t(t);
  struct tm tb;
  size_t const len(21);
  char buffer[len];
  TRI_gmtime(tt, &tb);
  ::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tb);
  return std::string(buffer, len - 1);
}

inline std::string timepointToString(std::chrono::system_clock::duration const& d) {
  return timepointToString(std::chrono::system_clock::time_point() + d);
}


  inline std::chrono::system_clock::time_point stringToTimepoint(std::string const& s) {
  if (!s.empty()) {
    try {
      std::tm tt;
      tt.tm_year = std::stoi(s.substr(0, 4)) - 1900;
      tt.tm_mon = std::stoi(s.substr(5, 2)) - 1;
      tt.tm_mday = std::stoi(s.substr(8, 2));
      tt.tm_hour = std::stoi(s.substr(11, 2));
      tt.tm_min = std::stoi(s.substr(14, 2));
      tt.tm_sec = std::stoi(s.substr(17, 2));
      tt.tm_isdst = 0;
      auto time_c = TRI_timegm(&tt);
      return std::chrono::system_clock::from_time_t(time_c);
    } catch (...) {}
  }
  return std::chrono::time_point<std::chrono::system_clock>();
}
