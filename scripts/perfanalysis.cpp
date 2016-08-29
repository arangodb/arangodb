// Compile with
//   g++ perfanalysis.cpp -o perfanalyis -std=c++11 -Wall -O3

#include <algorithm>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

using namespace std;

struct Event {
  static const regex re;
  string threadName;
  int tid;
  string cpu;
  double startTime;
  double duration;
  string name;
  string inbrackets;
  bool isRet;

  Event(string& line) : isRet(false) {
    std::smatch match_obj;
    if(!std::regex_search(line, match_obj, re)){
      throw std::logic_error("could not parse line");
    }

    threadName = match_obj[1];
    tid = std::stoi(match_obj[2]);
    cpu = match_obj[3];
    startTime = std::stod(match_obj[4]);
    duration = 0;
    name = match_obj[6];
    inbrackets = match_obj[7];
    if (match_obj[9].length() > 0) {
      isRet = true;
      name.erase(name.end() - 3, name.end());  // remove Ret suffix form name
    }
  }

  bool empty() { return name.empty(); }

  string id() { return to_string(tid) + name; }

  string pretty() {
    return to_string(duration) + " " + name + " " + to_string(startTime);
  }

  bool operator<(Event const& other) {
    if (name < other.name) {
      return true;
    } else if (name > other.name) {
      return false;
    } else {
      return duration < other.duration;
    }
  }
};

// sample output:
// arangod 32636 [005] 16678249.324973: probe_arangod:insertLocalRet: (14a7f60 <- 14a78d6)
// process tid   core  timepoint        scope:name                          frame
const regex Event::re(
    R"_(\s*(\S+))_"                                  // name 1
    R"_(\s+(\d+))_"                                  // tid  2
    R"_(\s+\[(\d+)\])_"                              // cup  3
    R"_(\s+(\d+\.\d+):)_"                            // time 4
    R"_(\s+([^: ]+):([^: ]+):)_"                     // scope:func 5:6
    R"_(\s+\(([0-9a-f]+)(\s+<-\s+([0-9a-f]+))?\))_"  // (start -> stop) 7 -> 9
    ,
    std::regex_constants::ECMAScript | std::regex_constants::optimize);

int main(int /*argc*/, char* /*argv*/ []) {
  unordered_map<string, unique_ptr<Event>> table;
  vector<unique_ptr<Event>> list;

  string line;
  while (getline(cin, line)) {
    auto event = std::make_unique<Event>(line);
    if (!event->empty()) {
      string id = event->id();
      // insert to table if it is not a function return
      if (!event->isRet) {
        auto it = table.find(id);
        if (it != table.end()) {
          cout << "Alarm, double event found:\n" << line << std::endl;
        } else {
          table.insert(make_pair(id, std::move(event)));
        }
      // update duration in table
      } else {
        auto it = table.find(id);
        if (it == table.end()) {
          cout << "Return for unknown event found:\n" << line << std::endl;
        } else {
          unique_ptr<Event> ev = std::move(it->second);
          table.erase(it);
          ev->duration = event->startTime - ev->startTime;
          list.push_back(std::move(ev));
        }
      }
    }
  }
  cout << "Unreturned events:\n";
  for (auto& p : table) {
    cout << p.second->pretty() << "\n";
  }
  sort(list.begin(), list.end(),
       [](unique_ptr<Event>const& a, unique_ptr<Event>const& b) -> bool { return *a < *b; });
  cout << "Events sorted by name and time:\n";
  for (auto& e : list) {
    cout << e->pretty() << "\n";
  }
  return 0;
}
