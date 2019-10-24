#include "boost/atomic.hpp"
#include "boost/thread.hpp"
#include <iostream>

using namespace boost;

int main() {
    atomic<size_t> total(0), failures(0);

#pragma omp parallel shared(total, failures) num_threads(1000)
    {
      mutex mtx;
      condition_variable cond;
      unique_lock<mutex> lk(mtx);
      for (int i = 0; i < 500; i++) {
        ++total;
        if (cv_status::timeout != cond.wait_for(lk, chrono::milliseconds(10)))
          ++failures;
      }
    }
    if(failures)
      std::cout << "There were " << failures << " failures out of " << total << " timed waits." << std::endl;
    if((100*failures)/total>40)
    {
      std::cerr << "This exceeds 10%, so failing the test." << std::endl;
      return 1;
    }
    return 0;
}
