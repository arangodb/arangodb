// Compile with
//   g++ perfanalysis.cpp -o perfanalyis -std=c++11 -Wall -O3

#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <algorithm>

using namespace std;

struct Event {
  string threadName;
  int tid;
  string cpu;
  double startTime;
  double duration;
  string name;
  string inbrackets;
  bool isRet;

  Event(string& line) : isRet(false) {
    char* s = strdup(line.c_str());
    char* p = strtok(s, " ");
    char* q;
    if (p != nullptr) {
      threadName = p;
      p = strtok(nullptr, " ");
      tid = strtol(p, nullptr, 10);
      p = strtok(nullptr, " ");
      cpu = p;
      p = strtok(nullptr, " ");
      startTime = strtod(p, nullptr);
      p = strtok(nullptr, " ");
      q = strtok(nullptr, " ");
      if (strcmp(q, "cs:") == 0) {
        free(s);
        return;
      }
      name = p;
      name.pop_back();
      auto l = name.size();
      if (l >= 3 && name[l-1] == 't' && name[l-2] == 'e' && 
          name[l-3] == 'R') {
        isRet = true;
        name.pop_back();
        name.pop_back();
        name.pop_back();
      }
      inbrackets = q;
    }
    free(s);
  }

  bool empty() {
    return name.empty();
  }

  string id() {
    return to_string(tid) + name;
  }

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

int main(int argc, char* argv[]) {
  unordered_map<string, Event*> table;
  vector<Event*> list;

  string line;
  while (getline(cin, line)) {
    Event* e = new Event(line);
    if (!e->empty()) {
      string id = e->id();
      if (!e->isRet) {
        auto it = table.find(id);
        if (it != table.end()) {
          cout << "Alarm, double event found:\n" << line << std::endl;
        } else {
          table.insert(make_pair(id, e));
        }
      } else {
        auto it = table.find(id);
        if (it == table.end()) {
          cout << "Return for unknown event found:\n" << line << std::endl;
        } else {
          Event* s = it->second;
          table.erase(it);
          s->duration = e->startTime - s->startTime;
          list.push_back(s);
        }
      }
    }
  }
  cout << "Unreturned events:\n";
  for (auto& p : table) {
    cout << p.second->pretty() << "\n";
    delete p.second;
  }
  sort(list.begin(), list.end(), [](Event* a, Event* b) -> bool {
                                   return *a < *b;
                                 });
  cout << "Events sorted by name and time:\n";
  for (auto* e : list) {
    cout << e->pretty() << "\n";
  }
  return 0;
}
