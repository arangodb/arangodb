//          Copyright Nat Goodspeed 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <algorithm>                // std::find_if()

#include <boost/fiber/all.hpp>
#include <boost/fiber/scheduler.hpp>

class Verbose {
public:
    Verbose( std::string const& d, std::string const& s="stop") :
        desc( d),
        stop( s) {
        std::cout << desc << " start" << std::endl;
    }

    ~Verbose() {
        std::cout << desc << ' ' << stop << std::endl;
    }

    Verbose( Verbose const&) = delete;
    Verbose & operator=( Verbose const&) = delete;

private:
    std::string     desc;
    std::string     stop;
};

//[priority_props
class priority_props : public boost::fibers::fiber_properties {
public:
    priority_props( boost::fibers::context * ctx):
        fiber_properties( ctx), /*< Your subclass constructor must accept a 
                                 [^[class_link context]*] and pass it to
                                 the `fiber_properties` constructor. >*/
        priority_( 0) {
    }

    int get_priority() const {
        return priority_; /*< Provide read access methods at your own discretion. >*/
    }

    // Call this method to alter priority, because we must notify
    // priority_scheduler of any change.
    void set_priority( int p) { /*<
            It's important to call `notify()` on any
            change in a property that can affect the
            scheduler's behavior. Therefore, such
            modifications should only be performed
            through an access method. >*/
        // Of course, it's only worth reshuffling the queue and all if we're
        // actually changing the priority.
        if ( p != priority_) {
            priority_ = p;
            notify();
        }
    }

    // The fiber name of course is solely for purposes of this example
    // program; it has nothing to do with implementing scheduler priority.
    // This is a public data member -- not requiring set/get access methods --
    // because we need not inform the scheduler of any change.
    std::string name; /*< A property that does not affect the scheduler does
                          not need access methods. >*/
private:
    int priority_;
};
//]

//[priority_scheduler
class priority_scheduler :
    public boost::fibers::algo::algorithm_with_properties< priority_props > {
private:
    typedef boost::fibers::scheduler::ready_queue_type/*< See [link ready_queue_t]. >*/   rqueue_t;

    rqueue_t                                rqueue_;
    std::mutex                  mtx_{};
    std::condition_variable     cnd_{};
    bool                        flag_{ false };

public:
    priority_scheduler() :
        rqueue_() {
    }

    // For a subclass of algorithm_with_properties<>, it's important to
    // override the correct awakened() overload.
    /*<< You must override the [member_link algorithm_with_properties..awakened]
         method. This is how your scheduler receives notification of a
         fiber that has become ready to run. >>*/
    virtual void awakened( boost::fibers::context * ctx, priority_props & props) noexcept {
        int ctx_priority = props.get_priority(); /*< `props` is the instance of
                                                   priority_props associated
                                                   with the passed fiber `ctx`. >*/
        // With this scheduler, fibers with higher priority values are
        // preferred over fibers with lower priority values. But fibers with
        // equal priority values are processed in round-robin fashion. So when
        // we're handed a new context*, put it at the end of the fibers
        // with that same priority. In other words: search for the first fiber
        // in the queue with LOWER priority, and insert before that one.
        rqueue_t::iterator i( std::find_if( rqueue_.begin(), rqueue_.end(),
            [ctx_priority,this]( boost::fibers::context & c)
            { return properties( &c ).get_priority() < ctx_priority; }));
        // Now, whether or not we found a fiber with lower priority,
        // insert this new fiber here.
        rqueue_.insert( i, * ctx);
//<-

        std::cout << "awakened(" << props.name << "): ";
        describe_ready_queue();
//->
    }

    /*<< You must override the [member_link algorithm_with_properties..pick_next]
         method. This is how your scheduler actually advises the fiber manager
         of the next fiber to run. >>*/
    virtual boost::fibers::context * pick_next() noexcept {
        // if ready queue is empty, just tell caller
        if ( rqueue_.empty() ) {
            return nullptr;
        }
        boost::fibers::context * ctx( & rqueue_.front() );
        rqueue_.pop_front();
//<-
        std::cout << "pick_next() resuming " << properties( ctx).name << ": ";
        describe_ready_queue();
//->
        return ctx;
    }

    /*<< You must override [member_link algorithm_with_properties..has_ready_fibers]
      to inform the fiber manager of the state of your ready queue. >>*/
    virtual bool has_ready_fibers() const noexcept {
        return ! rqueue_.empty();
    }

    /*<< Overriding [member_link algorithm_with_properties..property_change]
         is optional. This override handles the case in which the running
         fiber changes the priority of another ready fiber: a fiber already in
         our queue. In that case, move the updated fiber within the queue. >>*/
    virtual void property_change( boost::fibers::context * ctx, priority_props & props) noexcept {
        // Although our priority_props class defines multiple properties, only
        // one of them (priority) actually calls notify() when changed. The
        // point of a property_change() override is to reshuffle the ready
        // queue according to the updated priority value.
//<-
        std::cout << "property_change(" << props.name << '(' << props.get_priority()
                  << ")): ";
//->

        // 'ctx' might not be in our queue at all, if caller is changing the
        // priority of (say) the running fiber. If it's not there, no need to
        // move it: we'll handle it next time it hits awakened().
        if ( ! ctx->ready_is_linked()) { /*<
            Your `property_change()` override must be able to
            handle the case in which the passed `ctx` is not in
            your ready queue. It might be running, or it might be
            blocked. >*/
//<-
            // hopefully user will distinguish this case by noticing that
            // the fiber with which we were called does not appear in the
            // ready queue at all
            describe_ready_queue();
//->
            return;
        }

        // Found ctx: unlink it
        ctx->ready_unlink();

        // Here we know that ctx was in our ready queue, but we've unlinked
        // it. We happen to have a method that will (re-)add a context* to the
        // right place in the ready queue.
        awakened( ctx, props);
    }
//<-

    void describe_ready_queue() {
        if ( rqueue_.empty() ) {
            std::cout << "[empty]";
        } else {
            const char * delim = "";
            for ( boost::fibers::context & ctx : rqueue_) {
                priority_props & props( properties( & ctx) );
                std::cout << delim << props.name << '(' << props.get_priority() << ')';
                delim = ", ";
            }
        }
        std::cout << std::endl;
    }
//->

    void suspend_until( std::chrono::steady_clock::time_point const& time_point) noexcept {
        if ( (std::chrono::steady_clock::time_point::max)() == time_point) {
            std::unique_lock< std::mutex > lk( mtx_);
            cnd_.wait( lk, [this](){ return flag_; });
            flag_ = false;
        } else {
            std::unique_lock< std::mutex > lk( mtx_);
            cnd_.wait_until( lk, time_point, [this](){ return flag_; });
            flag_ = false;
        }
    }

    void notify() noexcept {
        std::unique_lock< std::mutex > lk( mtx_);
        flag_ = true;
        lk.unlock();
        cnd_.notify_all();
    }
};
//]

//[launch
template< typename Fn >
boost::fibers::fiber launch( Fn && func, std::string const& name, int priority) {
    boost::fibers::fiber fiber( func);
    priority_props & props( fiber.properties< priority_props >() );
    props.name = name;
    props.set_priority( priority);
    return fiber;
}
//]

void yield_fn() {
    std::string name( boost::this_fiber::properties< priority_props >().name);
    Verbose v( std::string("fiber ") + name);
    for ( int i = 0; i < 3; ++i) {
        std::cout << "fiber " << name << " yielding" << std::endl;
        boost::this_fiber::yield();
    }
}

void barrier_fn( boost::fibers::barrier & barrier) {
    std::string name( boost::this_fiber::properties< priority_props >().name);
    Verbose v( std::string("fiber ") + name);
    std::cout << "fiber " << name << " waiting on barrier" << std::endl;
    barrier.wait();
    std::cout << "fiber " << name << " yielding" << std::endl;
    boost::this_fiber::yield();
}

//[change_fn
void change_fn( boost::fibers::fiber & other,
                int other_priority,
                boost::fibers::barrier& barrier) {
    std::string name( boost::this_fiber::properties< priority_props >().name);
    Verbose v( std::string("fiber ") + name);

//<-
    std::cout << "fiber " << name << " waiting on barrier" << std::endl;
//->
    barrier.wait();
    // We assume a couple things about 'other':
    // - that it was also waiting on the same barrier
    // - that it has lower priority than this fiber.
    // If both are true, 'other' is now ready to run but is sitting in
    // priority_scheduler's ready queue. Change its priority.
    priority_props & other_props(
            other.properties< priority_props >() );
//<-
    std::cout << "fiber " << name << " changing priority of " << other_props.name
              << " to " << other_priority << std::endl;
//->
    other_props.set_priority( other_priority);
}
//]

//[main
int main( int argc, char *argv[]) {
    // make sure we use our priority_scheduler rather than default round_robin
    boost::fibers::use_scheduling_algorithm< priority_scheduler >();
/*=    ...*/
/*=}*/
//]
    Verbose v("main()");

    // for clarity
    std::cout << "main() setting name" << std::endl;
//[main_name
    boost::this_fiber::properties< priority_props >().name = "main";
//]
    std::cout << "main() running tests" << std::endl;

    {
        Verbose v("high-priority first", "stop\n");
        // verify that high-priority fiber always gets scheduled first
        boost::fibers::fiber low( launch( yield_fn, "low",    1) );
        boost::fibers::fiber med( launch( yield_fn, "medium", 2) );
        boost::fibers::fiber hi( launch( yield_fn,  "high",   3) );
        std::cout << "main: high.join()" << std::endl;
        hi.join();
        std::cout << "main: medium.join()" << std::endl;
        med.join();
        std::cout << "main: low.join()" << std::endl;
        low.join();
    }

    {
        Verbose v("same priority round-robin", "stop\n");
        // fibers of same priority are scheduled in round-robin order
        boost::fibers::fiber a( launch( yield_fn, "a", 0) );
        boost::fibers::fiber b( launch( yield_fn, "b", 0) );
        boost::fibers::fiber c( launch( yield_fn, "c", 0) );
        std::cout << "main: a.join()" << std::endl;
        a.join();
        std::cout << "main: b.join()" << std::endl;
        b.join();
        std::cout << "main: c.join()" << std::endl;
        c.join();
    }

    {
        Verbose v("barrier wakes up all", "stop\n");
        // using a barrier wakes up all waiting fibers at the same time
        boost::fibers::barrier barrier( 3);
        boost::fibers::fiber low( launch( [&barrier](){ barrier_fn( barrier); }, "low",    1) );
        boost::fibers::fiber med( launch( [&barrier](){ barrier_fn( barrier); }, "medium", 2) );
        boost::fibers::fiber hi( launch( [&barrier](){ barrier_fn( barrier); },  "high",   3) );
        std::cout << "main: low.join()" << std::endl;
        low.join();
        std::cout << "main: medium.join()" << std::endl;
        med.join();
        std::cout << "main: high.join()" << std::endl;
        hi.join();
    }

    {
        Verbose v("change priority", "stop\n");
        // change priority of a fiber in priority_scheduler's ready queue
        boost::fibers::barrier barrier( 3);
        boost::fibers::fiber c( launch( [&barrier](){ barrier_fn( barrier); }, "c", 1) );
        boost::fibers::fiber a( launch( [&c,&barrier]() { change_fn( c, 3, barrier); }, "a", 3) );
        boost::fibers::fiber b( launch( [&barrier](){ barrier_fn( barrier); }, "b", 2) );
        std::cout << "main: a.join()" << std::endl;
        std::cout << "main: a.join()" << std::endl;
        a.join();
        std::cout << "main: b.join()" << std::endl;
        b.join();
        std::cout << "main: c.join()" << std::endl;
        c.join();
    }

    std::cout << "done." << std::endl;

    return EXIT_SUCCESS;
}
