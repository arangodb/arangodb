#include "Metrics.h"
#include <type_traits>

Metrics::Metric::Metric(uint64_t val) {
  _var.emplace<0>(val);
}

Metrics::Metric::Metric(uint64_t buckets, uint64_t low, uint64_t high) {
  _var.emplace<1>(buckets);
}

Metrics::Metric::~Metric() {}

void Metrics::clear() {
  _registry.clear();
}

std::ostream& operator<< (std::ostream& o, Metrics::counter_type& s) {
  o << s.load();
  return o;
}

std::ostream& operator<< (std::ostream& o, Counter& s) {
  o << s.load();
  return o;
}

std::ostream& operator<< (std::ostream& o, Metrics::hist_type& v) {
  o << "[";
  for (size_t i = 0; i < v.size(); ++i) {
    if (i > 0) { o << ", "; }
    o << v.load(i);
  }
  o << "]";
  return o;
}

std::ostream& operator<< (std::ostream& o, Counter const& c) {
  return c.print(o);
}

Counter& Counter::operator++() {
  count();
  return *this;
}

Counter& Counter::operator++(int n) {
  count(n);
  return *this;
}

void Counter::count() {
  ++_b;
}

void Counter::count(uint64_t n) {
  _b += n;
}

std::ostream& Counter::print(std::ostream& o) const {
  _b.push();
  o << _c;
  return o;
}

uint64_t Counter::load() const {
  return _c.load();
}
Counter::Counter(Metrics::counter_type& c) : _c(c), _b(c) {}
Counter::~Counter() { _b.push(); }

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

std::ostream& operator<<(std::ostream& o, Metrics::var_type& v) {
  std::visit(overloaded {
      [&o](Metrics::counter_type& arg) { o << arg; },
      [&o](Metrics::hist_type& arg) { o << arg; },
    }, v);
  return o;
}

std::ostream& operator<< (std::ostream& o, Metrics::Metric const& c) {
  return c.print(o);
}

Metrics::~Metrics() {

  for (auto const& i : _registry) {
    std::cerr << i.first << ": " << i.second->_var << std::endl;
  }

}

Metrics::counter_type& Metrics::counter(
  std::string const& name, uint64_t val) {
  auto it = _registry.find(name);
  if (it != _registry.end()) {
    return std::get<Metrics::counter_type>(it->second->_var);
  }
  auto ptr = std::make_shared<Metrics::Metric>(val);
  _registry.emplace(name, ptr);
  return std::get<Metrics::counter_type>(ptr->_var);
}

Metrics::hist_type& Metrics::histogram(
  std::string const& name, uint64_t buckets) {
  auto it = _registry.find(name);
  if (it != _registry.end()) {
    return std::get<Metrics::hist_type>(it->second->_var);
  }
  auto ptr = std::make_shared<Metrics::Metric>(buckets, 0, 0);
  _registry.emplace(name, ptr);
  return std::get<Metrics::hist_type>(ptr->_var);
}


Metrics::counter_type& Metrics::counter(std::string const& name) {
  auto it = _registry.find(name);
  return std::get<Metrics::counter_type>(it->second->_var);
}

Metrics::hist_type& Metrics::histogram(std::string const& name) {
  auto it = _registry.find(name);
  return std::get<Metrics::hist_type>(it->second->_var);
}


template<class T> struct always_false : std::false_type {};

std::ostream& Metrics::Metric::print(std::ostream& o) const {
  std::visit(overloaded {
      [&o](Metrics::counter_type const& arg) { o << "counter with value "/* << arg*/; },
      [&o](Metrics::hist_type const& arg) { o << "histogram with value "/* << arg*/; },
    }, _var);
  return o;
}

