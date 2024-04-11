// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 1999-2015, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/


#include "simplethread.h"

#include <thread>
#include "unicode/utypes.h"
#include "intltest.h"


SimpleThread::SimpleThread() {
}

SimpleThread::~SimpleThread() {
    this->join();     // Avoid crashes if user neglected to join().
}

int SimpleThread::start() {
    fThread = std::thread(&SimpleThread::run, this);
    return fThread.joinable() ? 0 : 1;
}

void SimpleThread::join() {
    if (fThread.joinable()) {
        fThread.join();
    }
}



class ThreadPoolThread: public SimpleThread {
  public:
    ThreadPoolThread(ThreadPoolBase *pool, int32_t threadNum) : fPool(pool), fNum(threadNum) {}
    virtual void run() override { fPool->callFn(fNum); }
    ThreadPoolBase *fPool;
    int32_t         fNum;
};


ThreadPoolBase::ThreadPoolBase(IntlTest *test, int32_t howMany) :
        fIntlTest(test), fNumThreads(howMany), fThreads(nullptr) {
    fThreads = new SimpleThread *[fNumThreads];
    if (fThreads == nullptr) {
        fIntlTest->errln("%s:%d memory allocation failure.", __FILE__, __LINE__);
        return;
    }

    for (int i=0; i<fNumThreads; i++) {
        fThreads[i] = new ThreadPoolThread(this, i);
        if (fThreads[i] == nullptr) {
            fIntlTest->errln("%s:%d memory allocation failure.", __FILE__, __LINE__);
        }
    }
}

void ThreadPoolBase::start() {
    for (int i=0; i<fNumThreads; i++) {
        if (fThreads && fThreads[i]) {
            fThreads[i]->start();
        }
    }
}

void ThreadPoolBase::join() {
    for (int i=0; i<fNumThreads; i++) {
        if (fThreads && fThreads[i]) {
            fThreads[i]->join();
        }
    }
}

ThreadPoolBase::~ThreadPoolBase() {
    if (fThreads) {
        for (int i=0; i<fNumThreads; i++) {
            delete fThreads[i];
            fThreads[i] = nullptr;
        }
        delete[] fThreads;
        fThreads = nullptr;
    }
}
