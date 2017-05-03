// Compile with
//   g++ perfanalysis.cpp -o perfanalysis -std=c++14 -Wall -O3

#include <algorithm>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

struct StrTok {
  string& s;
  size_t pos;
  bool done;
  
  void advance() {
    while (pos < s.size() && s[pos] == ' ') {
      ++pos;
    }
    done = pos >= s.size();
  }

  StrTok(string& t) : s(t), pos(0), done(false) {
    advance();
  }

  const char* get() {
    if (done) {
      return nullptr;
    }
    auto p = s.find(' ', pos);
    if (p == string::npos) {
      done = true;
      return s.c_str() + pos;
    }
    s[p] = 0;
    auto ret = s.c_str() + pos;
    pos = p + 1;
    advance();
    return ret;
  }
};

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
    StrTok tok(line);
    const char* p = tok.get();
    const char* q;
    if (p != nullptr) {
      threadName = p;
      p = tok.get();
      tid = strtol(p, nullptr, 10);
      p = tok.get();
      cpu = p;
      p = tok.get();
      startTime = strtod(p, nullptr);
      p = tok.get();
      name = p;
      name.pop_back();
      q = tok.get();
      if (strcmp(q, "cs:") == 0) {
        return;
      }
      if (name.compare(0, 14, "probe_arangod:") == 0) {
        name = name.substr(14);
      }
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

int main(int /*argc*/, char* /*argv*/ []) {
  unordered_map<string, unique_ptr<Event>> table;
  vector<unique_ptr<Event>> list;

  string line;
  while (getline(cin, line)) {
    auto event = make_unique<Event>(line);
    if (!event->empty()) {
      string id = event->id();
      // insert to table if it is not a function return
      if (!event->isRet) {
        auto it = table.find(id);
        if (it != table.end()) {
          //cout << "Alarm, double event found:\n" << line << endl;
        } else {
          table.insert(make_pair(id, move(event)));
        }
      // update duration in table
      } else {
        auto it = table.find(id);
        if (it == table.end()) {
          cout << "Return for unknown event found:\n" << line << endl;
        } else {
          unique_ptr<Event> ev = move(it->second);
          table.erase(it);
          ev->duration = event->startTime - ev->startTime;
          list.push_back(move(ev));
        }
      }
    }
  }
  //cout << "Unreturned events:\n";
  //for (auto& p : table) {
  //  cout << p.second->pretty() << "\n";
  //}
  sort(list.begin(), list.end(),
       [](unique_ptr<Event>const& a, unique_ptr<Event>const& b) -> bool { return *a < *b; });
  cout << "Events sorted by name and time:\n";
  string current;
  size_t startPos = 0;
  bool bootstrap = true;
  auto printStats = [&](size_t i) -> void {
    cout << "Statistics in microseconds for " << current << ":\n"
      << "  Number of calls: " << i - startPos << "\n"
      << "  Minimal time   : " 
      << static_cast<uint64_t>(list[startPos]->duration * 1000000.0) << "\n";
    size_t pos = (i - startPos) / 2 + startPos;
    cout << "  50%ile         : " 
      << static_cast<uint64_t>(list[pos]->duration * 1000000.0) << "\n";
    pos = ((i - startPos) * 90) / 100 + startPos;
    cout << "  90%ile         : " 
      << static_cast<uint64_t>(list[pos]->duration * 1000000.0) << "\n";
    pos = ((i - startPos) * 99) / 100 + startPos;
    cout << "  99%ile         : " 
      << static_cast<uint64_t>(list[pos]->duration * 1000000.0) << "\n";
    pos = ((i - startPos) * 999) / 1000 + startPos;
    cout << "  99.9%ile       : " 
      << static_cast<uint64_t>(list[pos]->duration * 1000000.0) << "\n";
    cout << "  Maximal time   : " 
      << static_cast<uint64_t>(list[i-1]->duration * 1000000.0) << "\n";
    size_t j = (i-startPos > 30) ? i-30 : startPos;
    cout << "  Top " << i-j << " times   :";
    while (j < i) {
      cout << " " << static_cast<uint64_t>(list[j++]->duration * 1000000.0);
    }
    cout << "\n" << endl;
  };
  for (size_t i = 0; i < list.size(); ++i) {
    unique_ptr<Event>& e = list[i];
    if (e->name != current) {
      if (!bootstrap) {
        printStats(i);
      }
      bootstrap = false;
      startPos = i;
      current = e->name;
    }
    cout << e->pretty() << "\n";
  }
  printStats(list.size());
  return 0;
}
