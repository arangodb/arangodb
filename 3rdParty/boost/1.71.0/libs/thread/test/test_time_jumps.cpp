#ifdef _WIN32
	#include <windows.h>
#else
	#include <sys/time.h>
#endif

#include "boost/bind.hpp"
#include "boost/chrono.hpp"
#include "boost/chrono/ceil.hpp"
#include "boost/date_time.hpp"
#include "boost/thread/concurrent_queues/sync_priority_queue.hpp"
#include "boost/thread/concurrent_queues/sync_timed_queue.hpp"
#include "boost/thread/future.hpp"
#include "boost/thread/mutex.hpp"
#include "boost/thread/recursive_mutex.hpp"
#include "boost/thread/shared_lock_guard.hpp"
#include "boost/thread/shared_mutex.hpp"
#include "boost/thread/thread.hpp"

#include <iomanip>
#ifdef TEST_CPP14_FEATURES
#include <future>
#include <mutex>
#include <shared_mutex>
#include <thread>
#endif

/******************************************************************************/

/*
 * Summary:
 *
 * This code tests the behavior of time-related functions in the presence of
 * system clock changes (jumps). It requires root/Administrator privileges in
 * order to run because it changes the system clock. NTP should also be disabled
 * while running this code so that NTP can't change the system clock.
 *
 * Each function to be tested is executed five times. The amount of time the
 * function waits before returning is measured against the amount of time the
 * function was expected to wait. If the difference exceeds a threshold value
 * (defined below) then the test fails.
 *
 * The following values are intentially:
 * - more than 200 milliseconds
 * - more than 200 milliseconds apart
 * - not a multiple of 100 milliseconds
 * - not a multiple of each other
 * - don't sum or diff to a multiple of 100 milliseconds
 */
const long long s_waitMs            = 580;
const long long s_shortJumpMs       = 230;
const long long s_longJumpMs        = 870; // Causes additional, unavoidable failures when BOOST_THREAD_HAS_CONDATTR_SET_CLOCK_MONOTONIC is disabled
const long long s_sleepBeforeJumpMs = 110;

#ifdef _WIN32
const long long s_maxEarlyErrorMs =  10
                                  + 100; // Windows is unpredictable, especially in a VM, so allow extra time if the function returns early
const long long s_maxLateErrorMs  = 110  // due to polling, functions may not return for up to 100 milliseconds after they are supposed to
                                  + 100; // Windows is slow, especially in a VM, so allow extra time for the functions to return
#else
const long long s_maxEarlyErrorMs =  10;
const long long s_maxLateErrorMs  = 110; // Due to polling, functions may not return for up to 100 milliseconds after they are supposed to
#endif

int g_numTestsRun    = 0;
int g_numTestsPassed = 0;
int g_numTestsFailed = 0;

/******************************************************************************/

// A custom clock based off the system clock but with a different epoch.

namespace custom
{
    class custom_boost_clock
    {
    public:
        typedef boost::chrono::microseconds                   duration; // intentionally not nanoseconds
        typedef duration::rep                                 rep;
        typedef duration::period                              period;
        typedef boost::chrono::time_point<custom_boost_clock> time_point;
        static bool is_steady;

        static time_point now();
    };

    bool custom_boost_clock::is_steady = false;

    custom_boost_clock::time_point custom_boost_clock::now()
    {
        return time_point(boost::chrono::ceil<duration>(boost::chrono::system_clock::now().time_since_epoch()) - boost::chrono::hours(10 * 365 * 24));
    }

#ifdef TEST_CPP14_FEATURES
    class custom_std_clock
    {
    public:
        typedef std::chrono::microseconds                 duration; // intentionally not nanoseconds
        typedef duration::rep                             rep;
        typedef duration::period                          period;
        typedef std::chrono::time_point<custom_std_clock> time_point;
        static bool is_steady;

        static time_point now();
    };

    bool custom_std_clock::is_steady = false;

    custom_std_clock::time_point custom_std_clock::now()
    {
        return time_point(std::chrono::duration_cast<duration>(std::chrono::system_clock::now().time_since_epoch()) - std::chrono::hours(10 * 365 * 24));
    }
#endif
}

/******************************************************************************/

template <typename MutexType = boost::mutex, typename CondType = boost::condition_variable>
struct BoostHelper
{
    typedef MutexType mutex;
    typedef CondType cond;

    typedef boost::lock_guard<MutexType> lock_guard;
    typedef boost::unique_lock<MutexType> unique_lock;

    typedef boost::chrono::milliseconds milliseconds;
    typedef boost::chrono::nanoseconds nanoseconds;

    typedef boost::chrono::system_clock system_clock;
    typedef boost::chrono::steady_clock steady_clock;
    typedef custom::custom_boost_clock custom_clock;

    typedef system_clock::time_point system_time_point;
    typedef steady_clock::time_point steady_time_point;
    typedef custom_clock::time_point custom_time_point;

    typedef boost::cv_status cv_status;
    typedef boost::future_status future_status;

    typedef boost::packaged_task<bool> packaged_task;
    typedef boost::future<bool> future;
    typedef boost::shared_future<bool> shared_future;

    typedef boost::thread thread;

    static const milliseconds waitDur;

    template <typename T>
    static void sleep_for(T d)
    {
        boost::this_thread::sleep_for(d);
    }

    template <typename T>
    static void sleep_for_no_int(T d)
    {
        boost::this_thread::no_interruption_point::sleep_for(d);
    }

    template <typename T>
    static void sleep_until(T t)
    {
        boost::this_thread::sleep_until(t);
    }

    template <typename T>
    static void sleep_until_no_int(T t)
    {
        boost::this_thread::no_interruption_point::sleep_until(t);
    }

    static system_time_point systemNow()
    {
        return system_clock::now();
    }

    static steady_time_point steadyNow()
    {
        return steady_clock::now();
    }

    static custom_time_point customNow()
    {
        return custom_clock::now();
    }

    template <class ToDuration, class Rep, class Period>
    static ToDuration duration_cast(const boost::chrono::duration<Rep, Period>& d)
    {
        return boost::chrono::duration_cast<ToDuration>(d);
    }

    static milliseconds zero()
    {
        return milliseconds(0);
    }
};

template <typename MutexType, typename CondType>
const typename BoostHelper<MutexType, CondType>::milliseconds
BoostHelper<MutexType, CondType>::waitDur = typename BoostHelper<MutexType, CondType>::milliseconds(s_waitMs);

#ifdef TEST_CPP14_FEATURES
template <typename MutexType = std::mutex, typename CondType = std::condition_variable>
struct StdHelper
{
    typedef MutexType mutex;
    typedef CondType cond;

    typedef std::lock_guard<MutexType> lock_guard;
    typedef std::unique_lock<MutexType> unique_lock;

    typedef std::chrono::milliseconds milliseconds;
    typedef std::chrono::nanoseconds nanoseconds;

    typedef std::chrono::system_clock system_clock;
    typedef std::chrono::steady_clock steady_clock;
    typedef custom::custom_std_clock custom_clock;

    typedef system_clock::time_point system_time_point;
    typedef steady_clock::time_point steady_time_point;
    typedef custom_clock::time_point custom_time_point;

    typedef std::cv_status cv_status;
    typedef std::future_status future_status;

    typedef std::packaged_task<bool()> packaged_task;
    typedef std::future<bool> future;
    typedef std::shared_future<bool> shared_future;

    typedef std::thread thread;

    static const milliseconds waitDur;

    template <typename T>
    static void sleep_for(T d)
    {
        std::this_thread::sleep_for(d);
    }

    template <typename T>
    static void sleep_until(T t)
    {
        std::this_thread::sleep_until(t);
    }

    static system_time_point systemNow()
    {
        return system_clock::now();
    }

    static steady_time_point steadyNow()
    {
        return steady_clock::now();
    }

    static custom_time_point customNow()
    {
        return custom_clock::now();
    }

    template <class ToDuration, class Rep, class Period>
    static ToDuration duration_cast(const std::chrono::duration<Rep, Period>& d)
    {
        return std::chrono::duration_cast<ToDuration>(d);
    }

    static milliseconds zero()
    {
        return milliseconds(0);
    }
};

template <typename MutexType, typename CondType>
const typename StdHelper<MutexType, CondType>::milliseconds
StdHelper<MutexType, CondType>::waitDur = typename StdHelper<MutexType, CondType>::milliseconds(s_waitMs);
#endif

/******************************************************************************/

#ifdef _WIN32

void changeSystemTime(long long changeMs)
{
	Sleep(s_sleepBeforeJumpMs);

	SYSTEMTIME systemTime;
	GetSystemTime(&systemTime);

    FILETIME fileTime;
    if (!SystemTimeToFileTime(&systemTime, &fileTime))
    {
        std::cout << "ERROR: Couldn't convert system time to file time" << std::endl;
    }

    ULARGE_INTEGER largeInt;
    largeInt.LowPart  = fileTime.dwLowDateTime;
    largeInt.HighPart = fileTime.dwHighDateTime;
    largeInt.QuadPart += changeMs * 10000;
    fileTime.dwLowDateTime  = largeInt.LowPart;
    fileTime.dwHighDateTime = largeInt.HighPart;

    if (!FileTimeToSystemTime(&fileTime, &systemTime))
    {
        std::cout << "ERROR: Couldn't convert file time to system time" << std::endl;
    }

    if (!SetSystemTime(&systemTime))
    {
        std::cout << "ERROR: Couldn't set system time" << std::endl;
    }
}

#else

void changeSystemTime(long long changeMs)
{
    struct timespec sleepTs;
    sleepTs.tv_sec  = (s_sleepBeforeJumpMs / 1000);
    sleepTs.tv_nsec = (s_sleepBeforeJumpMs % 1000) * 1000000;
    nanosleep(&sleepTs, NULL);

    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0)
    {
        std::cout << "ERROR: Couldn't get system time" << std::endl;
    }

    changeMs += tv.tv_sec  * 1000;
    changeMs += tv.tv_usec / 1000;
    tv.tv_sec  = (changeMs / 1000);
    tv.tv_usec = (changeMs % 1000) * 1000;

    if (settimeofday(&tv, NULL) != 0)
    {
        std::cout << "ERROR: Couldn't set system time" << std::endl;
    }
}

#endif

enum RcEnum
{
    e_no_timeout,
    e_timeout,
    e_failed_bad,
    e_failed_good,
    e_succeeded_bad,
    e_succeeded_good,
    e_ready_bad,
    e_not_ready_good,
    e_na
};

template <typename Helper>
void checkWaitTime(typename Helper::nanoseconds expected, typename Helper::nanoseconds actual, RcEnum rc)
{
    if (expected != Helper::zero() && expected < typename Helper::milliseconds(s_sleepBeforeJumpMs))
    {
        expected = typename Helper::milliseconds(s_sleepBeforeJumpMs);
    }

    typename Helper::milliseconds expectedMs = Helper::template duration_cast<typename Helper::milliseconds>(expected);
    typename Helper::milliseconds actualMs   = Helper::template duration_cast<typename Helper::milliseconds>(actual);

    std::cout << "Expected: " << std::setw(4) << expectedMs.count() << " ms"
              << ", Actual: " << std::setw(4) << actualMs.count() << " ms"
              << ", Returned: ";
    switch (rc)
    {
        case e_no_timeout     : std::cout << "no_timeout, "; break;
        case e_timeout        : std::cout << "timeout,    "; break;
        case e_failed_bad     : std::cout << "failed,     "; break;
        case e_failed_good    : std::cout << "failed,     "; break;
        case e_succeeded_bad  : std::cout << "succeeded,  "; break;
        case e_succeeded_good : std::cout << "succeeded,  "; break;
        case e_ready_bad      : std::cout << "ready,      "; break;
        case e_not_ready_good : std::cout << "not_ready,  "; break;
        default               : std::cout << "N/A,        "; break;
    }

    if (expectedMs == Helper::zero())
    {
        std::cout << "FAILED: SKIPPED (test would lock up if run)";
        g_numTestsFailed++;
    }
    else if (actual < expected - typename Helper::milliseconds(s_maxEarlyErrorMs))
    {
        std::cout << "FAILED: TOO SHORT";
        if (rc == e_timeout) // bad
        {
            std::cout << ", RETURNED TIMEOUT";
        }
        else if (rc == e_failed_bad)
        {
            std::cout << ", RETURNED FAILED";
        }
        else if (rc == e_succeeded_bad)
        {
            std::cout << ", RETURNED SUCCEEDED";
        }
        else if (rc == e_ready_bad)
        {
            std::cout << ", RETURNED READY";
        }
        g_numTestsFailed++;
    }
    else if (actual > expected + typename Helper::milliseconds(s_maxLateErrorMs))
    {
        std::cout << "FAILED: TOO LONG";
        if (rc == e_no_timeout) // bad
        {
            std::cout << ", RETURNED NO_TIMEOUT";
        }
        else if (rc == e_failed_bad)
        {
            std::cout << ", RETURNED FAILED";
        }
        else if (rc == e_succeeded_bad)
        {
            std::cout << ", RETURNED SUCCEEDED";
        }
        else if (rc == e_ready_bad)
        {
            std::cout << ", RETURNED READY";
        }
        g_numTestsFailed++;
    }
    else if (rc == e_no_timeout) // bad
    {
        std::cout << "FAILED: RETURNED NO_TIMEOUT";
        g_numTestsFailed++;
    }
    else if (rc == e_failed_bad)
    {
        std::cout << "FAILED: RETURNED FAILED";
        g_numTestsFailed++;
    }
    else if (rc == e_succeeded_bad)
    {
        std::cout << "FAILED: RETURNED SUCCEEDED";
        g_numTestsFailed++;
    }
    else if (rc == e_ready_bad)
    {
        std::cout << "FAILED: RETURNED READY";
        g_numTestsFailed++;
    }
    else
    {
        std::cout << "Passed";
        g_numTestsPassed++;
    }
    std::cout << std::endl;

    g_numTestsRun++;
}

void sleepForLongTime()
{
#ifdef _WIN32
	Sleep(10000);
#else
    struct timespec ts = {5, 0};
    nanosleep(&ts, NULL);
#endif
}

bool returnFalse()
{
    return false;
}

/******************************************************************************/

// Run the test in the context provided, which may be the current thread or a separate thread.
template <typename Helper, typename Context, typename Function>
void runTestInContext(Context context, Function func, const std::string name)
{
    std::cout << name << ":" << std::endl;

    {
        std::cout << "    While system clock remains stable:        ";
        context(func, 0);
    }

    {
        std::cout << "    While system clock jumps back (short):    ";
        typename Helper::thread t(boost::bind(changeSystemTime, -s_shortJumpMs));
        context(func, -s_shortJumpMs);
        t.join();
    }

    {
        std::cout << "    While system clock jumps back (long):     ";
        typename Helper::thread t(boost::bind(changeSystemTime, -s_longJumpMs));
        context(func, -s_longJumpMs);
        t.join();
    }

    {
        std::cout << "    While system clock jumps forward (short): ";
        typename Helper::thread t(boost::bind(changeSystemTime, s_shortJumpMs));
        context(func, s_shortJumpMs);
        t.join();
    }

    {
        std::cout << "    While system clock jumps forward (long):  ";
        typename Helper::thread t(boost::bind(changeSystemTime, s_longJumpMs));
        context(func, s_longJumpMs);
        t.join();
    }
}

//--------------------------------------

template <typename Helper, typename Function>
void noThreadContext(Function func, const long long jumpMs)
{
    func(jumpMs);
}

template <typename Helper, typename Function>
void threadContextWithNone(Function func, const long long jumpMs)
{
    typename Helper::thread t(boost::bind(func, jumpMs));
    t.join();
}

template <typename Helper, typename Function>
void threadContextWithUnique(Function func, const long long jumpMs)
{
    typename Helper::mutex m;
    typename Helper::lock_guard g(m);
    typename Helper::thread t(boost::bind(func, boost::ref(m), jumpMs));
    t.join();
}

template <typename Helper, typename Function>
void threadContextWithShared(Function func, const long long jumpMs)
{
    typename Helper::mutex m;
    boost::shared_lock_guard<typename Helper::mutex> g(m);
    typename Helper::thread t(boost::bind(func, boost::ref(m), jumpMs));
    t.join();
}

template <typename Helper, typename Function>
void threadContextWithUpgrade(Function func, const long long jumpMs)
{
    typename Helper::mutex m;
    boost::upgrade_lock<typename Helper::mutex> g(m);
    typename Helper::thread t(boost::bind(func, boost::ref(m), jumpMs));
    t.join();
}

//--------------------------------------

// Run the test in the current thread.
template <typename Helper, typename Function>
void runTest(Function func, const std::string name)
{
    runTestInContext<Helper>(noThreadContext<Helper, Function>, func, name);
}

// Run the test in a separate thread.
template <typename Helper, typename Function>
void runTestWithNone(Function func, const std::string name)
{
    runTestInContext<Helper>(threadContextWithNone<Helper, Function>, func, name);
}

// Run the test in a separate thread. Pass a locked mutex to the function under test.
template <typename Helper, typename Function>
void runTestWithUnique(Function func, const std::string name)
{
    runTestInContext<Helper>(threadContextWithUnique<Helper, Function>, func, name);
}

// Run the test in a separate thread. Pass a shared-locked mutex to the function under test.
template <typename Helper, typename Function>
void runTestWithShared(Function func, const std::string name)
{
    runTestInContext<Helper>(threadContextWithShared<Helper, Function>, func, name);
}

// Run the test in a separate thread. Pass an upgrade-locked mutex to the function under test.
template <typename Helper, typename Function>
void runTestWithUpgrade(Function func, const std::string name)
{
    runTestInContext<Helper>(threadContextWithUpgrade<Helper, Function>, func, name);
}

/******************************************************************************/

// Test Sleep

template <typename Helper>
void testSleepFor(const long long jumpMs)
{
    typename Helper::steady_time_point before(Helper::steadyNow());
    Helper::sleep_for(Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, e_na);
}

template <typename Helper>
void testSleepUntilSteady(const long long jumpMs)
{
    typename Helper::steady_time_point before(Helper::steadyNow());
    Helper::sleep_until(Helper::steadyNow() + Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, e_na);
}

template <typename Helper>
void testSleepUntilSystem(const long long jumpMs)
{
    typename Helper::steady_time_point before(Helper::steadyNow());
    Helper::sleep_until(Helper::systemNow() + Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, e_na);
}

template <typename Helper>
void testSleepUntilCustom(const long long jumpMs)
{
    typename Helper::steady_time_point before(Helper::steadyNow());
    Helper::sleep_until(Helper::customNow() + Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, e_na);
}

//--------------------------------------

template <typename Helper>
void testSleepRelative(const long long jumpMs)
{
#ifndef SKIP_DATETIME_FUNCTIONS
    typename Helper::steady_time_point before(Helper::steadyNow());
    boost::posix_time::milliseconds ptDur(boost::chrono::duration_cast<boost::chrono::milliseconds>(Helper::waitDur).count());
    boost::this_thread::sleep(ptDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, e_na);
#else
    checkWaitTime<Helper>(Helper::zero(), Helper::zero(), e_na);
#endif
}

template <typename Helper>
void testSleepAbsolute(const long long jumpMs)
{
#ifndef SKIP_DATETIME_FUNCTIONS
    typename Helper::steady_time_point before(Helper::steadyNow());
    boost::posix_time::ptime ptNow(boost::posix_time::microsec_clock::universal_time());
    boost::posix_time::milliseconds ptDur(boost::chrono::duration_cast<boost::chrono::milliseconds>(Helper::waitDur).count());
    boost::this_thread::sleep(ptNow + ptDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, e_na);
#else
    checkWaitTime<Helper>(Helper::zero(), Helper::zero(), e_na);
#endif
}

//--------------------------------------

template <typename Helper>
void testSleepStd(const std::string& name)
{
    std::cout << std::endl;
    runTestWithNone<Helper>(testSleepFor        <Helper>, name + "::this_thread::sleep_for()");
    runTestWithNone<Helper>(testSleepUntilSteady<Helper>, name + "::this_thread::sleep_until(), steady time");
    runTestWithNone<Helper>(testSleepUntilSystem<Helper>, name + "::this_thread::sleep_until(), system time");
    runTestWithNone<Helper>(testSleepUntilCustom<Helper>, name + "::this_thread::sleep_until(), custom time");
}

template <typename Helper>
void testSleepBoost(const std::string& name)
{
    testSleepStd<Helper>(name);

    // Boost-only functions
    runTestWithNone<Helper>(testSleepRelative<Helper>, name + "::this_thread::sleep(), relative time");
    runTestWithNone<Helper>(testSleepAbsolute<Helper>, name + "::this_thread::sleep(), absolute time");
}

template <typename Helper>
void testSleepNoThreadStd(const std::string& name)
{
    std::cout << std::endl;
    runTest<Helper>(testSleepFor        <Helper>, name + "::this_thread::sleep_for(), no thread");
    runTest<Helper>(testSleepUntilSteady<Helper>, name + "::this_thread::sleep_until(), no thread, steady time");
    runTest<Helper>(testSleepUntilSystem<Helper>, name + "::this_thread::sleep_until(), no thread, system time");
    runTest<Helper>(testSleepUntilCustom<Helper>, name + "::this_thread::sleep_until(), no thread, custom time");
}

template <typename Helper>
void testSleepNoThreadBoost(const std::string& name)
{
    testSleepNoThreadStd<Helper>(name);

    // Boost-only functions
    runTest<Helper>(testSleepRelative<Helper>, name + "::this_thread::sleep(), no thread, relative time");
    runTest<Helper>(testSleepAbsolute<Helper>, name + "::this_thread::sleep(), no thread, absolute time");
}

/******************************************************************************/

// Test Sleep, No Interruption Point

template <typename Helper>
void testSleepForNoInt(const long long jumpMs)
{
    typename Helper::steady_time_point before(Helper::steadyNow());
    Helper::sleep_for_no_int(Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, e_na);
}

template <typename Helper>
void testSleepUntilNoIntSteady(const long long jumpMs)
{
    typename Helper::steady_time_point before(Helper::steadyNow());
    Helper::sleep_until_no_int(Helper::steadyNow() + Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, e_na);
}

template <typename Helper>
void testSleepUntilNoIntSystem(const long long jumpMs)
{
    typename Helper::steady_time_point before(Helper::steadyNow());
    Helper::sleep_until_no_int(Helper::systemNow() + Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, e_na);
}

template <typename Helper>
void testSleepUntilNoIntCustom(const long long jumpMs)
{
    typename Helper::steady_time_point before(Helper::steadyNow());
    Helper::sleep_until_no_int(Helper::customNow() + Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, e_na);
}

//--------------------------------------

#ifndef SKIP_NO_INT_SLEEP

template <typename Helper>
void testSleepNoIntRelative(const long long jumpMs)
{
#ifndef SKIP_DATETIME_FUNCTIONS
    typename Helper::steady_time_point before(Helper::steadyNow());
    boost::posix_time::milliseconds ptDur(boost::chrono::duration_cast<boost::chrono::milliseconds>(Helper::waitDur).count());
    boost::this_thread::no_interruption_point::sleep(ptDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, e_na);
#else
    checkWaitTime<Helper>(Helper::zero(), Helper::zero(), e_na);
#endif
}

template <typename Helper>
void testSleepNoIntAbsolute(const long long jumpMs)
{
#ifndef SKIP_DATETIME_FUNCTIONS
    typename Helper::steady_time_point before(Helper::steadyNow());
    boost::posix_time::ptime ptNow(boost::posix_time::microsec_clock::universal_time());
    boost::posix_time::milliseconds ptDur(boost::chrono::duration_cast<boost::chrono::milliseconds>(Helper::waitDur).count());
    boost::this_thread::no_interruption_point::sleep(ptNow + ptDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, e_na);
#else
    checkWaitTime<Helper>(Helper::zero(), Helper::zero(), e_na);
#endif
}

#endif

//--------------------------------------

// Only Boost supports no_interruption_point

template <typename Helper>
void testSleepNoIntBoost(const std::string& name)
{
    std::cout << std::endl;
    runTestWithNone<Helper>(testSleepForNoInt        <Helper>, name + "::this_thread::no_interruption_point::sleep_for()");
    runTestWithNone<Helper>(testSleepUntilNoIntSteady<Helper>, name + "::this_thread::no_interruption_point::sleep_until(), steady time");
    runTestWithNone<Helper>(testSleepUntilNoIntSystem<Helper>, name + "::this_thread::no_interruption_point::sleep_until(), system time");
    runTestWithNone<Helper>(testSleepUntilNoIntCustom<Helper>, name + "::this_thread::no_interruption_point::sleep_until(), custom time");

#ifndef SKIP_NO_INT_SLEEP
    runTestWithNone<Helper>(testSleepNoIntRelative<Helper>, name + "::this_thread::no_interruption_point::sleep(), relative time");
    runTestWithNone<Helper>(testSleepNoIntAbsolute<Helper>, name + "::this_thread::no_interruption_point::sleep(), absolute time");
#endif
}

template <typename Helper>
void testSleepNoThreadNoIntBoost(const std::string& name)
{
    std::cout << std::endl;
    runTest<Helper>(testSleepForNoInt        <Helper>, name + "::this_thread::no_interruption_point::sleep_for(), no thread");
    runTest<Helper>(testSleepUntilNoIntSteady<Helper>, name + "::this_thread::no_interruption_point::sleep_until(), no thread, steady time");
    runTest<Helper>(testSleepUntilNoIntSystem<Helper>, name + "::this_thread::no_interruption_point::sleep_until(), no thread, system time");
    runTest<Helper>(testSleepUntilNoIntCustom<Helper>, name + "::this_thread::no_interruption_point::sleep_until(), no thread, custom time");

#ifndef SKIP_NO_INT_SLEEP
    runTest<Helper>(testSleepNoIntRelative<Helper>, name + "::this_thread::no_interruption_point::sleep(), no thread, relative time");
    runTest<Helper>(testSleepNoIntAbsolute<Helper>, name + "::this_thread::no_interruption_point::sleep(), no thread, absolute time");
#endif
}

/******************************************************************************/

// Test Try Join

template <typename Helper>
void testTryJoinFor(const long long jumpMs)
{
    typename Helper::steady_time_point before(Helper::steadyNow());
    typename Helper::thread t3(sleepForLongTime);
    bool succeeded = t3.try_join_for(Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, succeeded ? e_succeeded_bad : e_failed_good);
}

template <typename Helper>
void testTryJoinUntilSteady(const long long jumpMs)
{
    typename Helper::steady_time_point before(Helper::steadyNow());
    typename Helper::thread t3(sleepForLongTime);
    bool succeeded = t3.try_join_until(Helper::steadyNow() + Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, succeeded ? e_succeeded_bad : e_failed_good);
}

template <typename Helper>
void testTryJoinUntilSystem(const long long jumpMs)
{
    typename Helper::steady_time_point before(Helper::steadyNow());
    typename Helper::thread t3(sleepForLongTime);
    bool succeeded = t3.try_join_until(Helper::systemNow() + Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, succeeded ? e_succeeded_bad : e_failed_good);
}

template <typename Helper>
void testTryJoinUntilCustom(const long long jumpMs)
{
    typename Helper::steady_time_point before(Helper::steadyNow());
    typename Helper::thread t3(sleepForLongTime);
    bool succeeded = t3.try_join_until(Helper::customNow() + Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, succeeded ? e_succeeded_bad : e_failed_good);
}

//--------------------------------------

template <typename Helper>
void testTimedJoinRelative(const long long jumpMs)
{
#ifndef SKIP_DATETIME_FUNCTIONS
    typename Helper::steady_time_point before(Helper::steadyNow());
    boost::posix_time::milliseconds ptDur(boost::chrono::duration_cast<boost::chrono::milliseconds>(Helper::waitDur).count());
    typename Helper::thread t3(sleepForLongTime);
    bool succeeded = t3.timed_join(ptDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, succeeded ? e_succeeded_bad : e_failed_good);
#else
    checkWaitTime<Helper>(Helper::zero(), Helper::zero(), e_na);
#endif
}

template <typename Helper>
void testTimedJoinAbsolute(const long long jumpMs)
{
#ifndef SKIP_DATETIME_FUNCTIONS
    typename Helper::steady_time_point before(Helper::steadyNow());
    boost::posix_time::ptime ptNow(boost::posix_time::microsec_clock::universal_time());
    boost::posix_time::milliseconds ptDur(boost::chrono::duration_cast<boost::chrono::milliseconds>(Helper::waitDur).count());
    typename Helper::thread t3(sleepForLongTime);
    bool succeeded = t3.timed_join(ptNow + ptDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, succeeded ? e_succeeded_bad : e_failed_good);
#else
    checkWaitTime<Helper>(Helper::zero(), Helper::zero(), e_na);
#endif
}

//--------------------------------------

// Only Boost supports timed try_join functions

template <typename Helper>
void testJoinBoost(const std::string& name)
{
    std::cout << std::endl;
    runTestWithNone<Helper>(testTryJoinFor        <Helper>, name + "::thread::try_join_for()");
    runTestWithNone<Helper>(testTryJoinUntilSteady<Helper>, name + "::thread::try_join_until(), steady time");
    runTestWithNone<Helper>(testTryJoinUntilSystem<Helper>, name + "::thread::try_join_until(), system time");
    runTestWithNone<Helper>(testTryJoinUntilCustom<Helper>, name + "::thread::try_join_until(), custom time");
    runTestWithNone<Helper>(testTimedJoinRelative <Helper>, name + "::thread::timed_join(), relative time");
    runTestWithNone<Helper>(testTimedJoinAbsolute <Helper>, name + "::thread::timed_join(), absolute time");
}

/******************************************************************************/

// Test Condition Variable Wait

template <typename Helper>
void testCondVarWaitFor(const long long jumpMs)
{
    typename Helper::cond cv;
    typename Helper::mutex m;
    typename Helper::unique_lock g(m);

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool noTimeout = (cv.wait_for(g, Helper::waitDur) == Helper::cv_status::no_timeout);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, noTimeout ? e_no_timeout : e_timeout);
}

template <typename Helper>
void testCondVarWaitUntilSteady(const long long jumpMs)
{
    typename Helper::cond cv;
    typename Helper::mutex m;
    typename Helper::unique_lock g(m);

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool noTimeout = (cv.wait_until(g, Helper::steadyNow() + Helper::waitDur) == Helper::cv_status::no_timeout);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, noTimeout ? e_no_timeout : e_timeout);
}

template <typename Helper>
void testCondVarWaitUntilSystem(const long long jumpMs)
{
    typename Helper::cond cv;
    typename Helper::mutex m;
    typename Helper::unique_lock g(m);

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool noTimeout = (cv.wait_until(g, Helper::systemNow() + Helper::waitDur) == Helper::cv_status::no_timeout);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, noTimeout ? e_no_timeout : e_timeout);
}

template <typename Helper>
void testCondVarWaitUntilCustom(const long long jumpMs)
{
    typename Helper::cond cv;
    typename Helper::mutex m;
    typename Helper::unique_lock g(m);

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool noTimeout = (cv.wait_until(g, Helper::customNow() + Helper::waitDur) == Helper::cv_status::no_timeout);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, noTimeout ? e_no_timeout : e_timeout);
}

//--------------------------------------

template <typename Helper>
void testCondVarTimedWaitRelative(const long long jumpMs)
{
#ifndef SKIP_DATETIME_FUNCTIONS
    typename Helper::cond cv;
    typename Helper::mutex m;
    typename Helper::unique_lock g(m);

    typename Helper::steady_time_point before(Helper::steadyNow());
    boost::posix_time::milliseconds ptDur(boost::chrono::duration_cast<boost::chrono::milliseconds>(Helper::waitDur).count());
    bool noTimeout = cv.timed_wait(g, ptDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, noTimeout ? e_no_timeout : e_timeout);
#else
    checkWaitTime<Helper>(Helper::zero(), Helper::zero(), e_na);
#endif
}

template <typename Helper>
void testCondVarTimedWaitAbsolute(const long long jumpMs)
{
#ifndef SKIP_DATETIME_FUNCTIONS
    typename Helper::cond cv;
    typename Helper::mutex m;
    typename Helper::unique_lock g(m);

    typename Helper::steady_time_point before(Helper::steadyNow());
    boost::posix_time::ptime ptNow(boost::posix_time::microsec_clock::universal_time());
    boost::posix_time::milliseconds ptDur(boost::chrono::duration_cast<boost::chrono::milliseconds>(Helper::waitDur).count());
    bool noTimeout = cv.timed_wait(g, ptNow + ptDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, noTimeout ? e_no_timeout : e_timeout);
#else
    checkWaitTime<Helper>(Helper::zero(), Helper::zero(), e_na);
#endif
}

//--------------------------------------

template <typename Helper>
void testCondVarStd(const std::string& name)
{
    std::cout << std::endl;
    runTestWithNone<Helper>(testCondVarWaitFor        <Helper>, name + "::wait_for()");
    runTestWithNone<Helper>(testCondVarWaitUntilSteady<Helper>, name + "::wait_until(), steady time");
    runTestWithNone<Helper>(testCondVarWaitUntilSystem<Helper>, name + "::wait_until(), system time");
    runTestWithNone<Helper>(testCondVarWaitUntilCustom<Helper>, name + "::wait_until(), custom time");
}

template <typename Helper>
void testCondVarBoost(const std::string& name)
{
    testCondVarStd<Helper>(name);

    // Boost-only functions
    runTestWithNone<Helper>(testCondVarTimedWaitRelative<Helper>, name + "::timed_wait(), relative time");
    runTestWithNone<Helper>(testCondVarTimedWaitAbsolute<Helper>, name + "::timed_wait(), absolute time");
}

/******************************************************************************/

// Test Condition Variable Wait with Predicate

template <typename Helper>
void testCondVarWaitForPred(const long long jumpMs)
{
    typename Helper::cond cv;
    typename Helper::mutex m;
    typename Helper::unique_lock g(m);

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool noTimeout = cv.wait_for(g, Helper::waitDur, returnFalse);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, noTimeout ? e_no_timeout : e_timeout);
}

template <typename Helper>
void testCondVarWaitUntilPredSteady(const long long jumpMs)
{
    typename Helper::cond cv;
    typename Helper::mutex m;
    typename Helper::unique_lock g(m);

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool noTimeout = cv.wait_until(g, Helper::steadyNow() + Helper::waitDur, returnFalse);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, noTimeout ? e_no_timeout : e_timeout);
}

template <typename Helper>
void testCondVarWaitUntilPredSystem(const long long jumpMs)
{
    typename Helper::cond cv;
    typename Helper::mutex m;
    typename Helper::unique_lock g(m);

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool noTimeout = cv.wait_until(g, Helper::systemNow() + Helper::waitDur, returnFalse);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, noTimeout ? e_no_timeout : e_timeout);
}

template <typename Helper>
void testCondVarWaitUntilPredCustom(const long long jumpMs)
{
    typename Helper::cond cv;
    typename Helper::mutex m;
    typename Helper::unique_lock g(m);

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool noTimeout = cv.wait_until(g, Helper::customNow() + Helper::waitDur, returnFalse);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, noTimeout ? e_no_timeout : e_timeout);
}

//--------------------------------------

template <typename Helper>
void testCondVarTimedWaitPredRelative(const long long jumpMs)
{
#ifndef SKIP_DATETIME_FUNCTIONS
    typename Helper::cond cv;
    typename Helper::mutex m;
    typename Helper::unique_lock g(m);

    typename Helper::steady_time_point before(Helper::steadyNow());
    boost::posix_time::milliseconds ptDur(boost::chrono::duration_cast<boost::chrono::milliseconds>(Helper::waitDur).count());
    bool noTimeout = cv.timed_wait(g, ptDur, returnFalse);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, noTimeout ? e_no_timeout : e_timeout);
#else
    checkWaitTime<Helper>(Helper::zero(), Helper::zero(), e_na);
#endif
}

template <typename Helper>
void testCondVarTimedWaitPredAbsolute(const long long jumpMs)
{
#ifndef SKIP_DATETIME_FUNCTIONS
    typename Helper::cond cv;
    typename Helper::mutex m;
    typename Helper::unique_lock g(m);

    typename Helper::steady_time_point before(Helper::steadyNow());
    boost::posix_time::ptime ptNow(boost::posix_time::microsec_clock::universal_time());
    boost::posix_time::milliseconds ptDur(boost::chrono::duration_cast<boost::chrono::milliseconds>(Helper::waitDur).count());
    bool noTimeout = cv.timed_wait(g, ptNow + ptDur, returnFalse);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, noTimeout ? e_no_timeout : e_timeout);
#else
    checkWaitTime<Helper>(Helper::zero(), Helper::zero(), e_na);
#endif
}

//--------------------------------------

template <typename Helper>
void testCondVarPredStd(const std::string& name)
{
    std::cout << std::endl;
    runTestWithNone<Helper>(testCondVarWaitForPred        <Helper>, name + "::wait_for(), with predicate");
    runTestWithNone<Helper>(testCondVarWaitUntilPredSteady<Helper>, name + "::wait_until(), with predicate, steady time");
    runTestWithNone<Helper>(testCondVarWaitUntilPredSystem<Helper>, name + "::wait_until(), with predicate, system time");
    runTestWithNone<Helper>(testCondVarWaitUntilPredCustom<Helper>, name + "::wait_until(), with predicate, custom time");
}

template <typename Helper>
void testCondVarPredBoost(const std::string& name)
{
    testCondVarPredStd<Helper>(name);

    // Boost-only functions
    runTestWithNone<Helper>(testCondVarTimedWaitPredRelative<Helper>, name + "::timed_wait(), with predicate, relative time");
    runTestWithNone<Helper>(testCondVarTimedWaitPredAbsolute<Helper>, name + "::timed_wait(), with predicate, absolute time");
}

/******************************************************************************/

// Test Try Lock

template <typename Helper>
void testTryLockFor(typename Helper::mutex& m, const long long jumpMs)
{
    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = m.try_lock_for(Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, succeeded ? e_succeeded_bad : e_failed_good);
}

template <typename Helper>
void testTryLockUntilSteady(typename Helper::mutex& m, const long long jumpMs)
{
    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = m.try_lock_until(Helper::steadyNow() + Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, succeeded ? e_succeeded_bad : e_failed_good);
}

template <typename Helper>
void testTryLockUntilSystem(typename Helper::mutex& m, const long long jumpMs)
{
    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = m.try_lock_until(Helper::systemNow() + Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, succeeded ? e_succeeded_bad : e_failed_good);
}

template <typename Helper>
void testTryLockUntilCustom(typename Helper::mutex& m, const long long jumpMs)
{
    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = m.try_lock_until(Helper::customNow() + Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, succeeded ? e_succeeded_bad : e_failed_good);
}

//--------------------------------------

template <typename Helper>
void testTimedLockRelative(typename Helper::mutex& m, const long long jumpMs)
{
#ifndef SKIP_DATETIME_FUNCTIONS
    typename Helper::steady_time_point before(Helper::steadyNow());
    boost::posix_time::milliseconds ptDur(boost::chrono::duration_cast<boost::chrono::milliseconds>(Helper::waitDur).count());
    bool succeeded = m.timed_lock(ptDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, succeeded ? e_succeeded_bad : e_failed_good);
#else
    checkWaitTime<Helper>(Helper::zero(), Helper::zero(), e_na);
#endif
}

template <typename Helper>
void testTimedLockAbsolute(typename Helper::mutex& m, const long long jumpMs)
{
#ifndef SKIP_DATETIME_FUNCTIONS
    typename Helper::steady_time_point before(Helper::steadyNow());
    boost::posix_time::ptime ptNow(boost::posix_time::microsec_clock::universal_time());
    boost::posix_time::milliseconds ptDur(boost::chrono::duration_cast<boost::chrono::milliseconds>(Helper::waitDur).count());
    bool succeeded = m.timed_lock(ptNow + ptDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, succeeded ? e_succeeded_bad : e_failed_good);
#else
    checkWaitTime<Helper>(Helper::zero(), Helper::zero(), e_na);
#endif
}

//--------------------------------------

template <typename Helper>
void testMutexStd(const std::string& name)
{
    std::cout << std::endl;
    runTestWithUnique<Helper>(testTryLockFor        <Helper>, name + "::try_lock_for()");
    runTestWithUnique<Helper>(testTryLockUntilSteady<Helper>, name + "::try_lock_until(), steady time");
    runTestWithUnique<Helper>(testTryLockUntilSystem<Helper>, name + "::try_lock_until(), system time");
    runTestWithUnique<Helper>(testTryLockUntilCustom<Helper>, name + "::try_lock_until(), custom time");
}

template <typename Helper>
void testMutexBoost(const std::string& name)
{
    testMutexStd<Helper>(name);

    // Boost-only functions
    runTestWithUnique<Helper>(testTimedLockRelative<Helper>, name + "::timed_lock(), relative time");
    runTestWithUnique<Helper>(testTimedLockAbsolute<Helper>, name + "::timed_lock(), absolute time");
}

/******************************************************************************/

// Test Try Lock Shared

template <typename Helper>
void testTryLockSharedFor(typename Helper::mutex& m, const long long jumpMs)
{
    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = m.try_lock_shared_for(Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, succeeded ? e_succeeded_bad : e_failed_good);
}

template <typename Helper>
void testTryLockSharedUntilSteady(typename Helper::mutex& m, const long long jumpMs)
{
    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = m.try_lock_shared_until(Helper::steadyNow() + Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, succeeded ? e_succeeded_bad : e_failed_good);
}

template <typename Helper>
void testTryLockSharedUntilSystem(typename Helper::mutex& m, const long long jumpMs)
{
    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = m.try_lock_shared_until(Helper::systemNow() + Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, succeeded ? e_succeeded_bad : e_failed_good);
}

template <typename Helper>
void testTryLockSharedUntilCustom(typename Helper::mutex& m, const long long jumpMs)
{
    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = m.try_lock_shared_until(Helper::customNow() + Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, succeeded ? e_succeeded_bad : e_failed_good);
}

//--------------------------------------

template <typename Helper>
void testTimedLockSharedRelative(typename Helper::mutex& m, const long long jumpMs)
{
#ifndef SKIP_DATETIME_FUNCTIONS
    typename Helper::steady_time_point before(Helper::steadyNow());
    boost::posix_time::milliseconds ptDur(boost::chrono::duration_cast<boost::chrono::milliseconds>(Helper::waitDur).count());
    bool succeeded = m.timed_lock_shared(ptDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, succeeded ? e_succeeded_bad : e_failed_good);
#else
    checkWaitTime<Helper>(Helper::zero(), Helper::zero(), e_na);
#endif
}

template <typename Helper>
void testTimedLockSharedAbsolute(typename Helper::mutex& m, const long long jumpMs)
{
#ifndef SKIP_DATETIME_FUNCTIONS
    typename Helper::steady_time_point before(Helper::steadyNow());
    boost::posix_time::ptime ptNow(boost::posix_time::microsec_clock::universal_time());
    boost::posix_time::milliseconds ptDur(boost::chrono::duration_cast<boost::chrono::milliseconds>(Helper::waitDur).count());
    bool succeeded = m.timed_lock_shared(ptNow + ptDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, succeeded ? e_succeeded_bad : e_failed_good);
#else
    checkWaitTime<Helper>(Helper::zero(), Helper::zero(), e_na);
#endif
}

//--------------------------------------

template <typename Helper>
void testMutexSharedStd(const std::string& name)
{
    std::cout << std::endl;
    runTestWithUnique<Helper>(testTryLockSharedFor        <Helper>, name + "::try_lock_shared_for()");
    runTestWithUnique<Helper>(testTryLockSharedUntilSteady<Helper>, name + "::try_lock_shared_until(), steady time");
    runTestWithUnique<Helper>(testTryLockSharedUntilSystem<Helper>, name + "::try_lock_shared_until(), system time");
    runTestWithUnique<Helper>(testTryLockSharedUntilCustom<Helper>, name + "::try_lock_shared_until(), custom time");
}

template <typename Helper>
void testMutexSharedBoost(const std::string& name)
{
    testMutexSharedStd<Helper>(name);

    // Boost-only functions
    runTestWithUnique<Helper>(testTimedLockSharedRelative<Helper>, name + "::timed_lock_shared(), relative time");
    runTestWithUnique<Helper>(testTimedLockSharedAbsolute<Helper>, name + "::timed_lock_shared(), absolute time");
}

/******************************************************************************/

// Test Try Lock Upgrade

#ifdef BOOST_THREAD_PROVIDES_SHARED_MUTEX_UPWARDS_CONVERSIONS

template <typename Helper>
void testTryLockUpgradeFor(typename Helper::mutex& m, const long long jumpMs)
{
    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = m.try_lock_upgrade_for(Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, succeeded ? e_succeeded_bad : e_failed_good);
}

template <typename Helper>
void testTryLockUpgradeUntilSteady(typename Helper::mutex& m, const long long jumpMs)
{
    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = m.try_lock_upgrade_until(Helper::steadyNow() + Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, succeeded ? e_succeeded_bad : e_failed_good);
}

template <typename Helper>
void testTryLockUpgradeUntilSystem(typename Helper::mutex& m, const long long jumpMs)
{
    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = m.try_lock_upgrade_until(Helper::systemNow() + Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, succeeded ? e_succeeded_bad : e_failed_good);
}

template <typename Helper>
void testTryLockUpgradeUntilCustom(typename Helper::mutex& m, const long long jumpMs)
{
    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = m.try_lock_upgrade_until(Helper::customNow() + Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, succeeded ? e_succeeded_bad : e_failed_good);
}

//--------------------------------------

template <typename Helper>
void testTimedLockUpgradeRelative(typename Helper::mutex& m, const long long jumpMs)
{
#ifndef SKIP_DATETIME_FUNCTIONS
    typename Helper::steady_time_point before(Helper::steadyNow());
    boost::posix_time::milliseconds ptDur(boost::chrono::duration_cast<boost::chrono::milliseconds>(Helper::waitDur).count());
    bool succeeded = m.timed_lock_upgrade(ptDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, succeeded ? e_succeeded_bad : e_failed_good);
#else
    checkWaitTime<Helper>(Helper::zero(), Helper::zero(), e_na);
#endif
}

template <typename Helper>
void testTimedLockUpgradeAbsolute(typename Helper::mutex& m, const long long jumpMs)
{
#ifndef SKIP_DATETIME_FUNCTIONS
    typename Helper::steady_time_point before(Helper::steadyNow());
    boost::posix_time::ptime ptNow(boost::posix_time::microsec_clock::universal_time());
    boost::posix_time::milliseconds ptDur(boost::chrono::duration_cast<boost::chrono::milliseconds>(Helper::waitDur).count());
    bool succeeded = m.timed_lock_upgrade(ptNow + ptDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, succeeded ? e_succeeded_bad : e_failed_good);
#else
    checkWaitTime<Helper>(Helper::zero(), Helper::zero(), e_na);
#endif
}

//--------------------------------------

template <typename Helper>
void testTryUnlockSharedAndLockFor(typename Helper::mutex& m, const long long jumpMs)
{
    boost::shared_lock_guard<typename Helper::mutex> g(m);

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = m.try_unlock_shared_and_lock_for(Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, succeeded ? e_succeeded_bad : e_failed_good);
}

template <typename Helper>
void testTryUnlockSharedAndLockUntilSteady(typename Helper::mutex& m, const long long jumpMs)
{
    boost::shared_lock_guard<typename Helper::mutex> g(m);

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = m.try_unlock_shared_and_lock_until(Helper::steadyNow() + Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, succeeded ? e_succeeded_bad : e_failed_good);
}

template <typename Helper>
void testTryUnlockSharedAndLockUntilSystem(typename Helper::mutex& m, const long long jumpMs)
{
    boost::shared_lock_guard<typename Helper::mutex> g(m);

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = m.try_unlock_shared_and_lock_until(Helper::systemNow() + Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, succeeded ? e_succeeded_bad : e_failed_good);
}

template <typename Helper>
void testTryUnlockSharedAndLockUntilCustom(typename Helper::mutex& m, const long long jumpMs)
{
    boost::shared_lock_guard<typename Helper::mutex> g(m);

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = m.try_unlock_shared_and_lock_until(Helper::customNow() + Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, succeeded ? e_succeeded_bad : e_failed_good);
}

//--------------------------------------

template <typename Helper>
void testTryUnlockUpgradeAndLockFor(typename Helper::mutex& m, const long long jumpMs)
{
    boost::upgrade_lock<typename Helper::mutex> g(m);

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = m.try_unlock_upgrade_and_lock_for(Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, succeeded ? e_succeeded_bad : e_failed_good);
}

template <typename Helper>
void testTryUnlockUpgradeAndLockUntilSteady(typename Helper::mutex& m, const long long jumpMs)
{
    boost::upgrade_lock<typename Helper::mutex> g(m);

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = m.try_unlock_upgrade_and_lock_until(Helper::steadyNow() + Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, succeeded ? e_succeeded_bad : e_failed_good);
}

template <typename Helper>
void testTryUnlockUpgradeAndLockUntilSystem(typename Helper::mutex& m, const long long jumpMs)
{
    boost::upgrade_lock<typename Helper::mutex> g(m);

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = m.try_unlock_upgrade_and_lock_until(Helper::systemNow() + Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, succeeded ? e_succeeded_bad : e_failed_good);
}

template <typename Helper>
void testTryUnlockUpgradeAndLockUntilCustom(typename Helper::mutex& m, const long long jumpMs)
{
    boost::upgrade_lock<typename Helper::mutex> g(m);

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = m.try_unlock_upgrade_and_lock_until(Helper::customNow() + Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, succeeded ? e_succeeded_bad : e_failed_good);
}

//--------------------------------------

template <typename Helper>
void testTryUnlockSharedAndLockUpgradeFor(typename Helper::mutex& m, const long long jumpMs)
{
    boost::shared_lock_guard<typename Helper::mutex> g(m);

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = m.try_unlock_shared_and_lock_upgrade_for(Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, succeeded ? e_succeeded_bad : e_failed_good);
}

template <typename Helper>
void testTryUnlockSharedAndLockUpgradeUntilSteady(typename Helper::mutex& m, const long long jumpMs)
{
    boost::shared_lock_guard<typename Helper::mutex> g(m);

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = m.try_unlock_shared_and_lock_upgrade_until(Helper::steadyNow() + Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, succeeded ? e_succeeded_bad : e_failed_good);
}

template <typename Helper>
void testTryUnlockSharedAndLockUpgradeUntilSystem(typename Helper::mutex& m, const long long jumpMs)
{
    boost::shared_lock_guard<typename Helper::mutex> g(m);

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = m.try_unlock_shared_and_lock_upgrade_until(Helper::systemNow() + Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, succeeded ? e_succeeded_bad : e_failed_good);
}

template <typename Helper>
void testTryUnlockSharedAndLockUpgradeUntilCustom(typename Helper::mutex& m, const long long jumpMs)
{
    boost::shared_lock_guard<typename Helper::mutex> g(m);

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = m.try_unlock_shared_and_lock_upgrade_until(Helper::customNow() + Helper::waitDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, succeeded ? e_succeeded_bad : e_failed_good);
}

#endif

//--------------------------------------

// Only Boost supports upgrade mutexes

template <typename Helper>
void testMutexUpgradeBoost(const std::string& name)
{
#ifdef BOOST_THREAD_PROVIDES_SHARED_MUTEX_UPWARDS_CONVERSIONS
    std::cout << std::endl;
    runTestWithUnique<Helper>(testTryLockUpgradeFor        <Helper>, name + "::try_lock_upgrade_for()");
    runTestWithUnique<Helper>(testTryLockUpgradeUntilSteady<Helper>, name + "::try_lock_upgrade_until(), steady time");
    runTestWithUnique<Helper>(testTryLockUpgradeUntilSystem<Helper>, name + "::try_lock_upgrade_until(), system time");
    runTestWithUnique<Helper>(testTryLockUpgradeUntilCustom<Helper>, name + "::try_lock_upgrade_until(), custom time");
    runTestWithUnique<Helper>(testTimedLockUpgradeRelative <Helper>, name + "::timed_lock_upgrade(), relative time");
    runTestWithUnique<Helper>(testTimedLockUpgradeAbsolute <Helper>, name + "::timed_lock_upgrade(), absolute time");

    std::cout << std::endl;
    runTestWithShared<Helper>(testTryUnlockSharedAndLockFor        <Helper>, name + "::try_unlock_shared_and_lock_for()");
    runTestWithShared<Helper>(testTryUnlockSharedAndLockUntilSteady<Helper>, name + "::try_unlock_shared_and_lock_until(), steady time");
    runTestWithShared<Helper>(testTryUnlockSharedAndLockUntilSystem<Helper>, name + "::try_unlock_shared_and_lock_until(), system time");
    runTestWithShared<Helper>(testTryUnlockSharedAndLockUntilCustom<Helper>, name + "::try_unlock_shared_and_lock_until(), custom time");

    std::cout << std::endl;
    runTestWithShared<Helper>(testTryUnlockUpgradeAndLockFor        <Helper>, name + "::try_unlock_upgrade_and_lock_for()");
    runTestWithShared<Helper>(testTryUnlockUpgradeAndLockUntilSteady<Helper>, name + "::try_unlock_upgrade_and_lock_until(), steady time");
    runTestWithShared<Helper>(testTryUnlockUpgradeAndLockUntilSystem<Helper>, name + "::try_unlock_upgrade_and_lock_until(), system time");
    runTestWithShared<Helper>(testTryUnlockUpgradeAndLockUntilCustom<Helper>, name + "::try_unlock_upgrade_and_lock_until(), custom time");

    std::cout << std::endl;
    runTestWithUpgrade<Helper>(testTryUnlockSharedAndLockFor        <Helper>, name + "::try_unlock_shared_and_lock_upgrade_for()");
    runTestWithUpgrade<Helper>(testTryUnlockSharedAndLockUntilSteady<Helper>, name + "::try_unlock_shared_and_lock_upgrade_until(), steady time");
    runTestWithUpgrade<Helper>(testTryUnlockSharedAndLockUntilSystem<Helper>, name + "::try_unlock_shared_and_lock_upgrade_until(), system time");
    runTestWithUpgrade<Helper>(testTryUnlockSharedAndLockUntilCustom<Helper>, name + "::try_unlock_shared_and_lock_upgrade_until(), custom time");
#endif
}

/******************************************************************************/

// Test Future Wait

template <typename Helper>
void testFutureWaitFor(const long long jumpMs)
{
    typename Helper::packaged_task pt(returnFalse);
    typename Helper::future f = pt.get_future();

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool timeout = (f.wait_for(Helper::waitDur) == Helper::future_status::timeout);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, timeout ? e_timeout : e_no_timeout);
}

template <typename Helper>
void testFutureWaitUntilSteady(const long long jumpMs)
{
    typename Helper::packaged_task pt(returnFalse);
    typename Helper::future f = pt.get_future();

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool timeout = (f.wait_until(Helper::steadyNow() + Helper::waitDur) == Helper::future_status::timeout);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, timeout ? e_timeout : e_no_timeout);
}

template <typename Helper>
void testFutureWaitUntilSystem(const long long jumpMs)
{
    typename Helper::packaged_task pt(returnFalse);
    typename Helper::future f = pt.get_future();

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool timeout = (f.wait_until(Helper::systemNow() + Helper::waitDur) == Helper::future_status::timeout);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, timeout ? e_timeout : e_no_timeout);
}

template <typename Helper>
void testFutureWaitUntilCustom(const long long jumpMs)
{
    typename Helper::packaged_task pt(returnFalse);
    typename Helper::future f = pt.get_future();

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool timeout = (f.wait_until(Helper::customNow() + Helper::waitDur) == Helper::future_status::timeout);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, timeout ? e_timeout : e_no_timeout);
}

//--------------------------------------

template <typename Helper>
void testFutureTimedWaitRelative(const long long jumpMs)
{
#ifndef SKIP_DATETIME_FUNCTIONS
    typename Helper::packaged_task pt(returnFalse);
    typename Helper::future f = pt.get_future();

    typename Helper::steady_time_point before(Helper::steadyNow());
    boost::posix_time::milliseconds ptDur(boost::chrono::duration_cast<boost::chrono::milliseconds>(Helper::waitDur).count());
    bool noTimeout = f.timed_wait(ptDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, noTimeout ? e_no_timeout : e_timeout);
#else
    checkWaitTime<Helper>(Helper::zero(), Helper::zero(), e_na);
#endif
}

template <typename Helper>
void testFutureTimedWaitAbsolute(const long long jumpMs)
{
#ifndef SKIP_DATETIME_FUNCTIONS
    typename Helper::packaged_task pt(returnFalse);
    typename Helper::future f = pt.get_future();

    typename Helper::steady_time_point before(Helper::steadyNow());
    boost::posix_time::ptime ptNow(boost::posix_time::microsec_clock::universal_time());
    boost::posix_time::milliseconds ptDur(boost::chrono::duration_cast<boost::chrono::milliseconds>(Helper::waitDur).count());
    bool noTimeout = f.timed_wait_until(ptNow + ptDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, noTimeout ? e_no_timeout : e_timeout);
#else
    checkWaitTime<Helper>(Helper::zero(), Helper::zero(), e_na);
#endif
}

//--------------------------------------

template <typename Helper>
void testFutureStd(const std::string& name)
{
    std::cout << std::endl;
    runTestWithNone<Helper>(testFutureWaitFor        <Helper>, name + "::wait_for()");
    runTestWithNone<Helper>(testFutureWaitUntilSteady<Helper>, name + "::wait_until(), steady time");
    runTestWithNone<Helper>(testFutureWaitUntilSystem<Helper>, name + "::wait_until(), system time");
    runTestWithNone<Helper>(testFutureWaitUntilCustom<Helper>, name + "::wait_until(), custom time");
}

template <typename Helper>
void testFutureBoost(const std::string& name)
{
    testFutureStd<Helper>(name);

    // Boost-only functions
    runTestWithNone<Helper>(testFutureTimedWaitRelative<Helper>, name + "::timed_wait(), relative time");
    runTestWithNone<Helper>(testFutureTimedWaitAbsolute<Helper>, name + "::timed_wait_until(), absolute time");
}

/******************************************************************************/

// Test Shared Future Wait

template <typename Helper>
void testSharedFutureWaitFor(const long long jumpMs)
{
    typename Helper::packaged_task pt(returnFalse);
    typename Helper::future f = pt.get_future();
    typename Helper::shared_future sf = boost::move(f);

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool timeout = (sf.wait_for(Helper::waitDur) == Helper::future_status::timeout);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, timeout ? e_timeout : e_no_timeout);
}

template <typename Helper>
void testSharedFutureWaitUntilSteady(const long long jumpMs)
{
    typename Helper::packaged_task pt(returnFalse);
    typename Helper::future f = pt.get_future();
    typename Helper::shared_future sf = boost::move(f);

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool timeout = (sf.wait_until(Helper::steadyNow() + Helper::waitDur) == Helper::future_status::timeout);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, timeout ? e_timeout : e_no_timeout);
}

template <typename Helper>
void testSharedFutureWaitUntilSystem(const long long jumpMs)
{
    typename Helper::packaged_task pt(returnFalse);
    typename Helper::future f = pt.get_future();
    typename Helper::shared_future sf = boost::move(f);

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool timeout = (sf.wait_until(Helper::systemNow() + Helper::waitDur) == Helper::future_status::timeout);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, timeout ? e_timeout : e_no_timeout);
}

template <typename Helper>
void testSharedFutureWaitUntilCustom(const long long jumpMs)
{
    typename Helper::packaged_task pt(returnFalse);
    typename Helper::future f = pt.get_future();
    typename Helper::shared_future sf = boost::move(f);

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool timeout = (sf.wait_until(Helper::customNow() + Helper::waitDur) == Helper::future_status::timeout);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, timeout ? e_timeout : e_no_timeout);
}

//--------------------------------------

template <typename Helper>
void testSharedFutureTimedWaitRelative(const long long jumpMs)
{
#ifndef SKIP_DATETIME_FUNCTIONS
    typename Helper::packaged_task pt(returnFalse);
    typename Helper::future f = pt.get_future();
    typename Helper::shared_future sf = boost::move(f);

    typename Helper::steady_time_point before(Helper::steadyNow());
    boost::posix_time::milliseconds ptDur(boost::chrono::duration_cast<boost::chrono::milliseconds>(Helper::waitDur).count());
    bool noTimeout = sf.timed_wait(ptDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, noTimeout ? e_no_timeout : e_timeout);
#else
    checkWaitTime<Helper>(Helper::zero(), Helper::zero(), e_na);
#endif
}

template <typename Helper>
void testSharedFutureTimedWaitAbsolute(const long long jumpMs)
{
#ifndef SKIP_DATETIME_FUNCTIONS
    typename Helper::packaged_task pt(returnFalse);
    typename Helper::future f = pt.get_future();
    typename Helper::shared_future sf = boost::move(f);

    typename Helper::steady_time_point before(Helper::steadyNow());
    boost::posix_time::ptime ptNow(boost::posix_time::microsec_clock::universal_time());
    boost::posix_time::milliseconds ptDur(boost::chrono::duration_cast<boost::chrono::milliseconds>(Helper::waitDur).count());
    bool noTimeout = sf.timed_wait_until(ptNow + ptDur);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, noTimeout ? e_no_timeout : e_timeout);
#else
    checkWaitTime<Helper>(Helper::zero(), Helper::zero(), e_na);
#endif
}

//--------------------------------------

template <typename Helper>
void testSharedFutureStd(const std::string& name)
{
    std::cout << std::endl;
    runTestWithNone<Helper>(testSharedFutureWaitFor        <Helper>, name + "::wait_for()");
    runTestWithNone<Helper>(testSharedFutureWaitUntilSteady<Helper>, name + "::wait_until(), steady time");
    runTestWithNone<Helper>(testSharedFutureWaitUntilSystem<Helper>, name + "::wait_until(), system time");
    runTestWithNone<Helper>(testSharedFutureWaitUntilCustom<Helper>, name + "::wait_until(), custom time");
}

template <typename Helper>
void testSharedFutureBoost(const std::string& name)
{
    testSharedFutureStd<Helper>(name);

    // Boost-only functions
    runTestWithNone<Helper>(testSharedFutureTimedWaitRelative<Helper>, name + "::timed_wait(), relative time");
    runTestWithNone<Helper>(testSharedFutureTimedWaitAbsolute<Helper>, name + "::timed_wait_until(), absolute time");
}

/******************************************************************************/

// Test Sync Priority Queue

template <typename Helper>
void testSyncPriorityQueuePullFor(const long long jumpMs)
{
    boost::sync_priority_queue<int> q;
    int i;

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool timeout = (q.pull_for(Helper::waitDur, i) == boost::queue_op_status::timeout);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, timeout ? e_timeout : e_no_timeout);
}

template <typename Helper>
void testSyncPriorityQueuePullUntilSteady(const long long jumpMs)
{
    boost::sync_priority_queue<int> q;
    int i;

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool timeout = (q.pull_until(Helper::steadyNow() + Helper::waitDur, i) == boost::queue_op_status::timeout);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, timeout ? e_timeout : e_no_timeout);
}

template <typename Helper>
void testSyncPriorityQueuePullUntilSystem(const long long jumpMs)
{
    boost::sync_priority_queue<int> q;
    int i;

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool timeout = (q.pull_until(Helper::systemNow() + Helper::waitDur, i) == boost::queue_op_status::timeout);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, timeout ? e_timeout : e_no_timeout);
}

template <typename Helper>
void testSyncPriorityQueuePullUntilCustom(const long long jumpMs)
{
    boost::sync_priority_queue<int> q;
    int i;

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool timeout = (q.pull_until(Helper::customNow() + Helper::waitDur, i) == boost::queue_op_status::timeout);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, timeout ? e_timeout : e_no_timeout);
}

//--------------------------------------

// Only Boost supports sync_priority_queue

template <typename Helper>
void testSyncPriorityQueueBoost(const std::string& name)
{
    std::cout << std::endl;
    runTestWithNone<Helper>(testSyncPriorityQueuePullFor        <Helper>, name + "::pull_for()");
    runTestWithNone<Helper>(testSyncPriorityQueuePullUntilSteady<Helper>, name + "::pull_until(), steady time");
    runTestWithNone<Helper>(testSyncPriorityQueuePullUntilSystem<Helper>, name + "::pull_until(), system time");
    runTestWithNone<Helper>(testSyncPriorityQueuePullUntilCustom<Helper>, name + "::pull_until(), custom time");
}

/******************************************************************************/

// Test Sync Timed Queue

template <typename Helper>
void testSyncTimedQueuePullForEmptySteady(const long long jumpMs)
{
    boost::sync_timed_queue<int, typename Helper::steady_clock> q;
    int i;

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool timeout = (q.pull_for(Helper::waitDur, i) == boost::queue_op_status::timeout);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, timeout ? e_timeout : e_no_timeout);
}

template <typename Helper>
void testSyncTimedQueuePullForEmptySystem(const long long jumpMs)
{
    boost::sync_timed_queue<int, typename Helper::system_clock> q;
    int i;

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool timeout = (q.pull_for(Helper::waitDur, i) == boost::queue_op_status::timeout);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, timeout ? e_timeout : e_no_timeout);
}

template <typename Helper>
void testSyncTimedQueuePullForEmptyCustom(const long long jumpMs)
{
    boost::sync_timed_queue<int, typename Helper::custom_clock> q;
    int i;

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool timeout = (q.pull_for(Helper::waitDur, i) == boost::queue_op_status::timeout);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, timeout ? e_timeout : e_no_timeout);
}

template <typename Helper>
void testSyncTimedQueuePullUntilEmptySteady(const long long jumpMs)
{
    boost::sync_timed_queue<int, typename Helper::steady_clock> q;
    int i;

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool timeout = (q.pull_until(Helper::steadyNow() + Helper::waitDur, i) == boost::queue_op_status::timeout);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, timeout ? e_timeout : e_no_timeout);
}

template <typename Helper>
void testSyncTimedQueuePullUntilEmptySystem(const long long jumpMs)
{
    boost::sync_timed_queue<int, typename Helper::system_clock> q;
    int i;

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool timeout = (q.pull_until(Helper::systemNow() + Helper::waitDur, i) == boost::queue_op_status::timeout);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, timeout ? e_timeout : e_no_timeout);
}

template <typename Helper>
void testSyncTimedQueuePullUntilEmptyCustom(const long long jumpMs)
{
    boost::sync_timed_queue<int, typename Helper::custom_clock> q;
    int i;

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool timeout = (q.pull_until(Helper::customNow() + Helper::waitDur, i) == boost::queue_op_status::timeout);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, timeout ? e_timeout : e_no_timeout);
}

//--------------------------------------

template <typename Helper>
void testSyncTimedQueuePullForNotReadySteady(const long long jumpMs)
{
    boost::sync_timed_queue<int, typename Helper::steady_clock> q;
    q.push(0, typename Helper::milliseconds(10000)); // a long time
    int i;

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool notReady = (q.pull_for(Helper::waitDur, i) == boost::queue_op_status::not_ready);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, notReady ? e_not_ready_good : e_ready_bad);
}

template <typename Helper>
void testSyncTimedQueuePullForNotReadySystem(const long long jumpMs)
{
    boost::sync_timed_queue<int, typename Helper::system_clock> q;
    q.push(0, typename Helper::milliseconds(10000)); // a long time
    int i;

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool notReady = (q.pull_for(Helper::waitDur, i) == boost::queue_op_status::not_ready);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, notReady ? e_not_ready_good : e_ready_bad);
}

template <typename Helper>
void testSyncTimedQueuePullForNotReadyCustom(const long long jumpMs)
{
    boost::sync_timed_queue<int, typename Helper::custom_clock> q;
    q.push(0, typename Helper::milliseconds(10000)); // a long time
    int i;

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool notReady = (q.pull_for(Helper::waitDur, i) == boost::queue_op_status::not_ready);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, notReady ? e_not_ready_good : e_ready_bad);
}

template <typename Helper>
void testSyncTimedQueuePullUntilNotReadySteady(const long long jumpMs)
{
    boost::sync_timed_queue<int, typename Helper::steady_clock> q;
    q.push(0, typename Helper::milliseconds(10000)); // a long time
    int i;

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool notReady = (q.pull_until(Helper::steadyNow() + Helper::waitDur, i) == boost::queue_op_status::not_ready);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, notReady ? e_not_ready_good : e_ready_bad);
}

template <typename Helper>
void testSyncTimedQueuePullUntilNotReadySystem(const long long jumpMs)
{
    boost::sync_timed_queue<int, typename Helper::system_clock> q;
    q.push(0, typename Helper::milliseconds(10000)); // a long time
    int i;

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool notReady = (q.pull_until(Helper::systemNow() + Helper::waitDur, i) == boost::queue_op_status::not_ready);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, notReady ? e_not_ready_good : e_ready_bad);
}

template <typename Helper>
void testSyncTimedQueuePullUntilNotReadyCustom(const long long jumpMs)
{
    boost::sync_timed_queue<int, typename Helper::custom_clock> q;
    q.push(0, typename Helper::milliseconds(10000)); // a long time
    int i;

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool notReady = (q.pull_until(Helper::customNow() + Helper::waitDur, i) == boost::queue_op_status::not_ready);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, notReady ? e_not_ready_good : e_ready_bad);
}

//--------------------------------------

template <typename Helper>
void testSyncTimedQueuePullForSucceedsSteady(const long long jumpMs)
{
    boost::sync_timed_queue<int, typename Helper::steady_clock> q;
    q.push(0, Helper::waitDur);
    int i;

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = (q.pull_for(typename Helper::milliseconds(10000), i) == boost::queue_op_status::success);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, succeeded ? e_succeeded_good : e_failed_bad);
}

template <typename Helper>
void testSyncTimedQueuePullForSucceedsSystem(const long long jumpMs)
{
    boost::sync_timed_queue<int, typename Helper::system_clock> q;
    q.push(0, Helper::waitDur);
    int i;

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = (q.pull_for(typename Helper::milliseconds(10000), i) == boost::queue_op_status::success);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, succeeded ? e_succeeded_good : e_failed_bad);
}

template <typename Helper>
void testSyncTimedQueuePullForSucceedsCustom(const long long jumpMs)
{
    boost::sync_timed_queue<int, typename Helper::custom_clock> q;
    q.push(0, Helper::waitDur);
    int i;

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = (q.pull_for(typename Helper::milliseconds(10000), i) == boost::queue_op_status::success);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, succeeded ? e_succeeded_good : e_failed_bad);
}

template <typename Helper>
void testSyncTimedQueuePullUntilSucceedsSteady(const long long jumpMs)
{
    boost::sync_timed_queue<int, typename Helper::steady_clock> q;
    q.push(0, Helper::steadyNow() + Helper::waitDur);
    int i;

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = (q.pull_until(Helper::steadyNow() + typename Helper::milliseconds(10000), i) == boost::queue_op_status::success);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, succeeded ? e_succeeded_good : e_failed_bad);
}

template <typename Helper>
void testSyncTimedQueuePullUntilSucceedsSystem(const long long jumpMs)
{
    boost::sync_timed_queue<int, typename Helper::system_clock> q;
    q.push(0, Helper::systemNow() + Helper::waitDur);
    int i;

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = (q.pull_until(Helper::systemNow() + typename Helper::milliseconds(10000), i) == boost::queue_op_status::success);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, succeeded ? e_succeeded_good : e_failed_bad);
}

template <typename Helper>
void testSyncTimedQueuePullUntilSucceedsCustom(const long long jumpMs)
{
    boost::sync_timed_queue<int, typename Helper::custom_clock> q;
    q.push(0, Helper::customNow() + Helper::waitDur);
    int i;

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = (q.pull_until(Helper::customNow() + typename Helper::milliseconds(10000), i) == boost::queue_op_status::success);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, succeeded ? e_succeeded_good : e_failed_bad);
}

//--------------------------------------

template <typename Helper>
void testSyncTimedQueueWaitPullSucceedsSteady(const long long jumpMs)
{
    boost::sync_timed_queue<int, typename Helper::steady_clock> q;
    q.push(0, Helper::steadyNow() + Helper::waitDur);
    int i;

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = (q.wait_pull(i) == boost::queue_op_status::success);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur, after - before, succeeded ? e_succeeded_good : e_failed_bad);
}

template <typename Helper>
void testSyncTimedQueueWaitPullSucceedsSystem(const long long jumpMs)
{
    boost::sync_timed_queue<int, typename Helper::system_clock> q;
    q.push(0, Helper::systemNow() + Helper::waitDur);
    int i;

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = (q.wait_pull(i) == boost::queue_op_status::success);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, succeeded ? e_succeeded_good : e_failed_bad);
}

template <typename Helper>
void testSyncTimedQueueWaitPullSucceedsCustom(const long long jumpMs)
{
    boost::sync_timed_queue<int, typename Helper::custom_clock> q;
    q.push(0, Helper::customNow() + Helper::waitDur);
    int i;

    typename Helper::steady_time_point before(Helper::steadyNow());
    bool succeeded = (q.wait_pull(i) == boost::queue_op_status::success);
    typename Helper::steady_time_point after(Helper::steadyNow());

    checkWaitTime<Helper>(Helper::waitDur - typename Helper::milliseconds(jumpMs), after - before, succeeded ? e_succeeded_good : e_failed_bad);
}

//--------------------------------------

// Only Boost supports sync_timed_queue

template <typename Helper>
void testSyncTimedQueueBoost(const std::string& name)
{
    std::cout << std::endl;
    runTestWithNone<Helper>(testSyncTimedQueuePullForEmptySteady  <Helper>, name + "::pull_for(), empty, steady time");
    runTestWithNone<Helper>(testSyncTimedQueuePullForEmptySystem  <Helper>, name + "::pull_for(), empty, system time");
    runTestWithNone<Helper>(testSyncTimedQueuePullForEmptyCustom  <Helper>, name + "::pull_for(), empty, custom time");
    runTestWithNone<Helper>(testSyncTimedQueuePullUntilEmptySteady<Helper>, name + "::pull_until(), empty, steady time");
    runTestWithNone<Helper>(testSyncTimedQueuePullUntilEmptySystem<Helper>, name + "::pull_until(), empty, system time");
    runTestWithNone<Helper>(testSyncTimedQueuePullUntilEmptyCustom<Helper>, name + "::pull_until(), empty, custom time");

    std::cout << std::endl;
    runTestWithNone<Helper>(testSyncTimedQueuePullForNotReadySteady  <Helper>, name + "::pull_for(), not ready, steady time");
    runTestWithNone<Helper>(testSyncTimedQueuePullForNotReadySystem  <Helper>, name + "::pull_for(), not ready, system time");
    runTestWithNone<Helper>(testSyncTimedQueuePullForNotReadyCustom  <Helper>, name + "::pull_for(), not ready, custom time");
    runTestWithNone<Helper>(testSyncTimedQueuePullUntilNotReadySteady<Helper>, name + "::pull_until(), not ready, steady time");
    runTestWithNone<Helper>(testSyncTimedQueuePullUntilNotReadySystem<Helper>, name + "::pull_until(), not ready, system time");
    runTestWithNone<Helper>(testSyncTimedQueuePullUntilNotReadyCustom<Helper>, name + "::pull_until(), not ready, custom time");

    std::cout << std::endl;
    runTestWithNone<Helper>(testSyncTimedQueuePullForSucceedsSteady  <Helper>, name + "::pull_for(), succeeds, steady time");
    runTestWithNone<Helper>(testSyncTimedQueuePullForSucceedsSystem  <Helper>, name + "::pull_for(), succeeds, system time");
    runTestWithNone<Helper>(testSyncTimedQueuePullForSucceedsCustom  <Helper>, name + "::pull_for(), succeeds, custom time");
    runTestWithNone<Helper>(testSyncTimedQueuePullUntilSucceedsSteady<Helper>, name + "::pull_until(), succeeds, steady time");
    runTestWithNone<Helper>(testSyncTimedQueuePullUntilSucceedsSystem<Helper>, name + "::pull_until(), succeeds, system time");
    runTestWithNone<Helper>(testSyncTimedQueuePullUntilSucceedsCustom<Helper>, name + "::pull_until(), succeeds, custom time");

    std::cout << std::endl;
    runTestWithNone<Helper>(testSyncTimedQueueWaitPullSucceedsSteady<Helper>, name + "::wait_pull(), succeeds, steady time");
    runTestWithNone<Helper>(testSyncTimedQueueWaitPullSucceedsSystem<Helper>, name + "::wait_pull(), succeeds, system time");
    runTestWithNone<Helper>(testSyncTimedQueueWaitPullSucceedsCustom<Helper>, name + "::wait_pull(), succeeds, custom time");
}

/******************************************************************************/

int main()
{
    std::cout << std::endl;
    std::cout << "INFO: This test requires root/Administrator privileges in order to change the system clock." << std::endl;
    std::cout << "INFO: Disable NTP while running this test to prevent NTP from changing the system clock." << std::endl;
    std::cout << std::endl;

    std::cout << "BOOST_HAS_PTHREAD_DELAY_NP:                             ";
#ifdef BOOST_HAS_PTHREAD_DELAY_NP
    std::cout << "YES" << std::endl;
#else
    std::cout << "NO" << std::endl;
#endif

    std::cout << "BOOST_HAS_NANOSLEEP:                                    ";
#ifdef BOOST_HAS_NANOSLEEP
    std::cout << "YES" << std::endl;
#else
    std::cout << "NO" << std::endl;
#endif

    std::cout << "BOOST_THREAD_SLEEP_FOR_IS_STEADY:                       ";
#ifdef BOOST_THREAD_SLEEP_FOR_IS_STEADY
    std::cout << "YES" << std::endl;
#else
    std::cout << "NO" << std::endl;
#endif

    std::cout << "CLOCK_MONOTONIC:                                        ";
#ifdef CLOCK_MONOTONIC
    std::cout << "YES" << std::endl;
#else
    std::cout << "NO" << std::endl;
#endif

    std::cout << "BOOST_THREAD_PROVIDES_GENERIC_SHARED_MUTEX_ON_WIN:      ";
#ifdef BOOST_THREAD_PROVIDES_GENERIC_SHARED_MUTEX_ON_WIN
    std::cout << "YES" << std::endl;
#else
    std::cout << "NO" << std::endl;
#endif

    std::cout << "BOOST_THREAD_PROVIDES_SHARED_MUTEX_UPWARDS_CONVERSIONS: ";
#ifdef BOOST_THREAD_PROVIDES_SHARED_MUTEX_UPWARDS_CONVERSIONS
    std::cout << "YES" << std::endl;
#else
    std::cout << "NO" << std::endl;
#endif

    std::cout << "BOOST_THREAD_V2_SHARED_MUTEX:                           ";
#ifdef BOOST_THREAD_V2_SHARED_MUTEX
    std::cout << "YES" << std::endl;
#else
    std::cout << "NO" << std::endl;
#endif

    std::cout << "BOOST_THREAD_HAS_CONDATTR_SET_CLOCK_MONOTONIC:          ";
#ifdef BOOST_THREAD_HAS_CONDATTR_SET_CLOCK_MONOTONIC
    std::cout << "YES" << std::endl;
#else
    std::cout << "NO" << std::endl;
#endif

    std::cout << std::endl;
    std::cout << "Wait Time:              " << s_waitMs            << " ms" << std::endl;
    std::cout << "Short Jump Time:        " << s_shortJumpMs       << " ms" << std::endl;
    std::cout << "Long Jump Time:         " << s_longJumpMs        << " ms" << std::endl;
    std::cout << "Sleep Before Jump Time: " << s_sleepBeforeJumpMs << " ms" << std::endl;
    std::cout << "Max Early Error:        " << s_maxEarlyErrorMs   << " ms" << std::endl;
    std::cout << "Max Late Error:         " << s_maxLateErrorMs    << " ms" << std::endl;

    testSleepBoost             <BoostHelper<                                           > >("boost");
    testSleepNoIntBoost        <BoostHelper<                                           > >("boost");
    testSleepNoThreadBoost     <BoostHelper<                                           > >("boost");
    testSleepNoThreadNoIntBoost<BoostHelper<                                           > >("boost");
    testJoinBoost              <BoostHelper<                                           > >("boost");
    testCondVarBoost           <BoostHelper<boost::mutex, boost::condition_variable    > >("boost::condition_variable");
    testCondVarPredBoost       <BoostHelper<boost::mutex, boost::condition_variable    > >("boost::condition_variable");
    testCondVarBoost           <BoostHelper<boost::mutex, boost::condition_variable_any> >("boost::condition_variable_any");
    testCondVarPredBoost       <BoostHelper<boost::mutex, boost::condition_variable_any> >("boost::condition_variable_any");
    testMutexBoost             <BoostHelper<boost::timed_mutex                         > >("boost::timed_mutex");
    testMutexBoost             <BoostHelper<boost::recursive_timed_mutex               > >("boost::recursive_timed_mutex");
    testMutexBoost             <BoostHelper<boost::shared_mutex                        > >("boost::shared_mutex"); // upgrade_mutex is a typedef of shared_mutex, so no need to test upgrade_mutex
    testMutexSharedBoost       <BoostHelper<boost::shared_mutex                        > >("boost::shared_mutex"); // upgrade_mutex is a typedef of shared_mutex, so no need to test upgrade_mutex
    testMutexUpgradeBoost      <BoostHelper<boost::shared_mutex                        > >("boost::shared_mutex"); // upgrade_mutex is a typedef of shared_mutex, so no need to test upgrade_mutex
    testFutureBoost            <BoostHelper<                                           > >("boost::future");
    testSharedFutureBoost      <BoostHelper<                                           > >("boost::shared_future");
    testSyncPriorityQueueBoost <BoostHelper<                                           > >("boost::sync_priority_queue");
    testSyncTimedQueueBoost    <BoostHelper<                                           > >("boost::sync_timed_queue");

#ifdef TEST_CPP14_FEATURES
    testSleepStd        <StdHelper<                                       > >("std");
    testSleepNoThreadStd<StdHelper<                                       > >("std");
    testCondVarStd      <StdHelper<std::mutex, std::condition_variable    > >("std::condition_variable");
    testCondVarPredStd  <StdHelper<std::mutex, std::condition_variable    > >("std::condition_variable");
    testCondVarStd      <StdHelper<std::mutex, std::condition_variable_any> >("std::condition_variable_any");
    testCondVarPredStd  <StdHelper<std::mutex, std::condition_variable_any> >("std::condition_variable_any");
    testMutexStd        <StdHelper<std::timed_mutex                       > >("std::timed_mutex");
    testMutexStd        <StdHelper<std::recursive_timed_mutex             > >("std::recursive_timed_mutex");
    testMutexStd        <StdHelper<std::shared_timed_mutex                > >("std::shared_timed_mutex");
    testMutexSharedStd  <StdHelper<std::shared_timed_mutex                > >("std::shared_timed_mutex");
    testFutureStd       <StdHelper<                                       > >("std::future");
    testSharedFutureStd <StdHelper<                                       > >("std::shared_future");
#endif

    std::cout << std::endl;
    std::cout << "Number of Tests Run:    " << g_numTestsRun << std::endl;
    std::cout << "Number of Tests Passed: " << g_numTestsPassed << std::endl;
    std::cout << "Number of Tests Failed: " << g_numTestsFailed << std::endl;

    return 0;
}
