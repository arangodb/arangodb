// interval-set.h

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Copyright 2005-2010 Google, Inc.
// Author: riley@google.com (Michael Riley)
//
// \file
// Class to represent and operate on sets of intervals.

#ifndef FST_LIB_INTERVAL_SET_H_
#define FST_LIB_INTERVAL_SET_H_

#include <iostream>
#include <vector>
using std::vector;


#include <fst/util.h>


namespace fst {

// Stores and operates on a set of half-open integral intervals [a,b)
// of signed integers of type T.
template <typename T>
class IntervalSet {
 public:
  struct Interval {
    T begin;
    T end;

    Interval() : begin(-1), end(-1) {}

    Interval(T b, T e) : begin(b), end(e) {}

    bool operator<(const Interval &i) const {
      return begin < i.begin || (begin == i.begin && end > i.end);
    }

    bool operator==(const Interval &i) const {
      return begin == i.begin && end == i.end;
    }

    bool operator!=(const Interval &i) const {
      return begin != i.begin || end != i.end;
    }

    istream &Read(istream &strm) {
      T n;
      ReadType(strm, &n);
      begin = n;
      ReadType(strm, &n);
      end = n;
      return strm;
    }

    ostream &Write(ostream &strm) const {
      T n = begin;
      WriteType(strm, n);
      n = end;
      WriteType(strm, n);
      return strm;
    }
  };

  IntervalSet() : count_(-1) {}

  // Returns the interval set as a vector.
  vector<Interval> *Intervals() { return &intervals_; }

  const vector<Interval> *Intervals() const { return &intervals_; }

  bool Empty() const { return intervals_.empty(); }

  T Size() const { return intervals_.size(); }

  // Number of points in the intervals (undefined if not normalized).
  T Count() const { return count_; }

  void Clear() {
    intervals_.clear();
    count_ = 0;
  }

  // Adds an interval set to the set. The result may not be normalized.
  void Union(const IntervalSet<T> &iset) {
    const vector<Interval> *intervals = iset.Intervals();
    for (typename vector<Interval>::const_iterator it = intervals->begin();
         it != intervals->end(); ++it)
      intervals_.push_back(*it);
  }

  // Requires intervals be normalized.
  bool Member(T value) const {
    Interval interval(value, value);
    typename vector<Interval>::const_iterator lb =
        std::lower_bound(intervals_.begin(), intervals_.end(), interval);
    if (lb == intervals_.begin())
      return false;
    return (--lb)->end > value;
  }

  // Requires intervals be normalized.
  bool operator==(const IntervalSet<T>& iset) const {
    return *(iset.Intervals()) == intervals_;
  }

  // Requires intervals be normalized.
  bool operator!=(const IntervalSet<T>& iset) const {
    return *(iset.Intervals()) != intervals_;
  }

  bool Singleton() const {
    return intervals_.size() == 1 &&
        intervals_[0].begin + 1 == intervals_[0].end;
  }


  // Sorts; collapses overlapping and adjacent interals; sets count.
  void Normalize();

  // Intersects an interval set with the set. Requires intervals be
  // normalized. The result is normalized.
  void Intersect(const IntervalSet<T> &iset, IntervalSet<T> *oset) const;

  // Complements the set w.r.t [0, maxval). Requires intervals be
  // normalized. The result is normalized.
  void Complement(T maxval, IntervalSet<T> *oset) const;

  // Subtract an interval set from the set. Requires intervals be
  // normalized. The result is normalized.
  void Difference(const IntervalSet<T> &iset, IntervalSet<T> *oset) const;

  // Determines if an interval set overlaps with the set. Requires
  // intervals be normalized.
  bool Overlaps(const IntervalSet<T> &iset) const;

  // Determines if an interval set overlaps with the set but neither
  // is contained in the other. Requires intervals be normalized.
  bool StrictlyOverlaps(const IntervalSet<T> &iset) const;

  // Determines if an interval set is contained within the set. Requires
  // intervals be normalized.
  bool Contains(const IntervalSet<T> &iset) const;

  istream &Read(istream &strm) {
    ReadType(strm, &intervals_);
    return ReadType(strm, &count_);
  }

  ostream &Write(ostream &strm) const {
    WriteType(strm, intervals_);
    return WriteType(strm, count_);
  }

 private:
  vector<Interval> intervals_;
  T count_;
};

// Sorts; collapses overlapping and adjacent interavls; sets count.
template <typename T>
void IntervalSet<T>::Normalize() {
  std::sort(intervals_.begin(), intervals_.end());

  count_ = 0;
  T size = 0;
  for (T i = 0; i < intervals_.size(); ++i) {
    Interval &inti = intervals_[i];
    if (inti.begin == inti.end)
      continue;
    for (T j = i + 1; j < intervals_.size(); ++j) {
      Interval &intj = intervals_[j];
      if (intj.begin > inti.end)
        break;
      if (intj.end > inti.end)
        inti.end = intj.end;
      ++i;
    }
    count_ += inti.end - inti.begin;
    intervals_[size++] = inti;
  }
  intervals_.resize(size);
}

// Intersects an interval set with the set. Requires intervals be normalized.
// The result is normalized.
template <typename T>
void IntervalSet<T>::Intersect(const IntervalSet<T> &iset,
                               IntervalSet<T> *oset) const {
  const vector<Interval> *iintervals = iset.Intervals();
  vector<Interval> *ointervals = oset->Intervals();
  typename vector<Interval>::const_iterator it1 = intervals_.begin();
  typename vector<Interval>::const_iterator it2 = iintervals->begin();

  ointervals->clear();
  oset->count_ = 0;

  while (it1 != intervals_.end() && it2 != iintervals->end()) {
    if (it1->end <= it2->begin) {
      ++it1;
    } else if (it2->end <= it1->begin) {
      ++it2;
    } else {
      Interval interval;
      interval.begin = max(it1->begin, it2->begin);
      interval.end = min(it1->end, it2->end);
      ointervals->push_back(interval);
      oset->count_ += interval.end - interval.begin;
      if ((it1->end) < (it2->end))
        ++it1;
      else
        ++it2;
    }
  }
}

// Complements the set w.r.t [0, maxval). Requires intervals be normalized.
// The result is normalized.
template <typename T>
void IntervalSet<T>::Complement(T maxval, IntervalSet<T> *oset) const {
  vector<Interval> *ointervals = oset->Intervals();
  ointervals->clear();
  oset->count_ = 0;

  Interval interval;
  interval.begin = 0;
  for (typename vector<Interval>::const_iterator it = intervals_.begin();
       it != intervals_.end();
       ++it) {
    interval.end = min(it->begin, maxval);
    if ((interval.begin) < (interval.end)) {
      ointervals->push_back(interval);
      oset->count_ += interval.end - interval.begin;
    }
    interval.begin = it->end;
  }
  interval.end = maxval;
  if ((interval.begin) < (interval.end)) {
    ointervals->push_back(interval);
    oset->count_ += interval.end - interval.begin;
  }
}

// Subtract an interval set from the set. Requires intervals be normalized.
// The result is normalized.
template <typename T>
void IntervalSet<T>::Difference(const IntervalSet<T> &iset,
                                IntervalSet<T> *oset) const {
  if (intervals_.empty()) {
    oset->Intervals()->clear();
    oset->count_ = 0;
  } else {
    IntervalSet<T> cset;
    iset.Complement(intervals_.back().end, &cset);
    Intersect(cset, oset);
  }
}

// Determines if an interval set overlaps with the set. Requires
// intervals be normalized.
template <typename T>
bool IntervalSet<T>::Overlaps(const IntervalSet<T> &iset) const {
  const vector<Interval> *intervals = iset.Intervals();
  typename vector<Interval>::const_iterator it1 = intervals_.begin();
  typename vector<Interval>::const_iterator it2 = intervals->begin();

  while (it1 != intervals_.end() && it2 != intervals->end()) {
    if (it1->end <= it2->begin) {
      ++it1;
    } else if (it2->end <= it1->begin) {
      ++it2;
    } else {
      return true;
    }
  }
  return false;
}

// Determines if an interval set overlaps with the set but neither
// is contained in the other. Requires intervals be normalized.
template <typename T>
bool IntervalSet<T>::StrictlyOverlaps(const IntervalSet<T> &iset) const {
  const vector<Interval> *intervals = iset.Intervals();
  typename vector<Interval>::const_iterator it1 = intervals_.begin();
  typename vector<Interval>::const_iterator it2 = intervals->begin();
  bool only1 = false;   // point in intervals_ but not intervals
  bool only2 = false;   // point in intervals but not intervals_
  bool overlap = false; // point in both intervals_ and intervals

  while (it1 != intervals_.end() && it2 != intervals->end()) {
    if (it1->end <= it2->begin) {  // no overlap - it1 first
      only1 = true;
      ++it1;
    } else if (it2->end <= it1->begin) {  // no overlap - it2 first
      only2 = true;
      ++it2;
    } else if (it2->begin == it1->begin && it2->end == it1->end) {  // equals
      overlap = true;
      ++it1;
      ++it2;
    } else if (it2->begin <= it1->begin && it2->end >= it1->end) {  // 1 c 2
      only2 = true;
      overlap = true;
      ++it1;
    } else if (it1->begin <= it2->begin && it1->end >= it2->end) {  // 2 c 1
      only1 = true;
      overlap = true;
      ++it2;
    } else {  // strict overlap
      only1 = true;
      only2 = true;
      overlap = true;
    }
    if (only1 == true && only2 == true && overlap == true)
      return true;
  }
  if (it1 != intervals_.end())
    only1 = true;
  if (it2 != intervals->end())
    only2 = true;

  return only1 == true && only2 == true && overlap == true;
}

// Determines if an interval set is contained within the set. Requires
// intervals be normalized.
template <typename T>
bool IntervalSet<T>::Contains(const IntervalSet<T> &iset) const {
  if (iset.Count() > Count())
    return false;

  const vector<Interval> *intervals = iset.Intervals();
  typename vector<Interval>::const_iterator it1 = intervals_.begin();
  typename vector<Interval>::const_iterator it2 = intervals->begin();

  while (it1 != intervals_.end() && it2 != intervals->end()) {
    if ((it1->end) <= (it2->begin)) {  // no overlap - it1 first
      ++it1;
    } else if ((it2->begin) < (it1->begin) ||
               (it2->end) > (it1->end)) {  // no C
      return false;
    } else if (it2->end == it1->end) {
      ++it1;
      ++it2;
    } else {
      ++it2;
    }
  }
  return it2 == intervals->end();
}

template <typename T>
ostream &operator<<(ostream &strm, const IntervalSet<T> &s)  {
  typedef typename IntervalSet<T>::Interval Interval;
  const vector<Interval> *intervals = s.Intervals();
  strm << "{";
  for (typename vector<Interval>::const_iterator it = intervals->begin();
       it != intervals->end();
       ++it) {
    if (it != intervals->begin())
      strm << ",";
    strm << "[" << it->begin << "," << it->end << ")";
  }
  strm << "}";
  return strm;
}

}  // namespace fst

#endif  // FST_LIB_INTERVAL_SET_H_
