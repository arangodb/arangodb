#pragma once

#include <chrono>
#include <string>
#include <Logger/Logger.h>

namespace arangodb { namespace Utilities {

struct timer {
  using ns = ::std::chrono::nanoseconds;
  using clock = ::std::chrono::high_resolution_clock;
  using point = ::std::chrono::time_point<clock,ns>;
  std::string _name;
  point _start;
  bool _released;

  timer(std::string const& name = "unnamed")
    : _name(" - " + name)
    , _start(clock::now())
    , _released(false)
    {}

  ~timer(){
    if(!_released){
      release();
    }
  }

  void release(){
    LOG_DEVEL << "timer" << _name << ":" << std::fixed << diff(_start, clock::now()) / (double) std::giga::num << "s";
    _released = true;
  }

  std::uint64_t diff(point const& start, point const& end){
    return (end-start).count();
  }
};

}}
