#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/defer.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/system_executor.hpp>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <typeinfo>
#include <vector>

using boost::asio::any_io_executor;
using boost::asio::defer;
using boost::asio::post;
using boost::asio::strand;
using boost::asio::system_executor;

//------------------------------------------------------------------------------
// A tiny actor framework
// ~~~~~~~~~~~~~~~~~~~~~~

class actor;

// Used to identify the sender and recipient of messages.
typedef actor* actor_address;

// Base class for all registered message handlers.
class message_handler_base
{
public:
  virtual ~message_handler_base() {}

  // Used to determine which message handlers receive an incoming message.
  virtual const std::type_info& message_id() const = 0;
};

// Base class for a handler for a specific message type.
template <class Message>
class message_handler : public message_handler_base
{
public:
  // Handle an incoming message.
  virtual void handle_message(Message msg, actor_address from) = 0;
};

// Concrete message handler for a specific message type.
template <class Actor, class Message>
class mf_message_handler : public message_handler<Message>
{
public:
  // Construct a message handler to invoke the specified member function.
  mf_message_handler(void (Actor::* mf)(Message, actor_address), Actor* a)
    : function_(mf), actor_(a)
  {
  }

  // Used to determine which message handlers receive an incoming message.
  virtual const std::type_info& message_id() const
  {
    return typeid(Message);
  }

  // Handle an incoming message.
  virtual void handle_message(Message msg, actor_address from)
  {
    (actor_->*function_)(std::move(msg), from);
  }

  // Determine whether the message handler represents the specified function.
  bool is_function(void (Actor::* mf)(Message, actor_address)) const
  {
    return mf == function_;
  }

private:
  void (Actor::* function_)(Message, actor_address);
  Actor* actor_;
};

// Base class for all actors.
class actor
{
public:
  virtual ~actor()
  {
  }

  // Obtain the actor's address for use as a message sender or recipient.
  actor_address address()
  {
    return this;
  }

  // Send a message from one actor to another.
  template <class Message>
  friend void send(Message msg, actor_address from, actor_address to)
  {
    // Execute the message handler in the context of the target's executor.
    post(to->executor_,
      [=, msg=std::move(msg)]() mutable
      {
        to->call_handler(std::move(msg), from);
      });
  }

protected:
  // Construct the actor to use the specified executor for all message handlers.
  actor(any_io_executor e)
    : executor_(std::move(e))
  {
  }

  // Register a handler for a specific message type. Duplicates are permitted.
  template <class Actor, class Message>
  void register_handler(void (Actor::* mf)(Message, actor_address))
  {
    handlers_.push_back(
      std::make_shared<mf_message_handler<Actor, Message>>(
        mf, static_cast<Actor*>(this)));
  }

  // Deregister a handler. Removes only the first matching handler.
  template <class Actor, class Message>
  void deregister_handler(void (Actor::* mf)(Message, actor_address))
  {
    const std::type_info& id = typeid(Message);
    for (auto iter = handlers_.begin(); iter != handlers_.end(); ++iter)
    {
      if ((*iter)->message_id() == id)
      {
        auto mh = static_cast<mf_message_handler<Actor, Message>*>(iter->get());
        if (mh->is_function(mf))
        {
          handlers_.erase(iter);
          return;
        }
      }
    }
  }

  // Send a message from within a message handler.
  template <class Message>
  void tail_send(Message msg, actor_address to)
  {
    // Execute the message handler in the context of the target's executor.
    defer(to->executor_,
      [=, msg=std::move(msg), from=this]
      {
        to->call_handler(std::move(msg), from);
      });
  }

private:
  // Find the matching message handlers, if any, and call them.
  template <class Message>
  void call_handler(Message msg, actor_address from)
  {
    const std::type_info& message_id = typeid(Message);
    for (auto& h: handlers_)
    {
      if (h->message_id() == message_id)
      {
        auto mh = static_cast<message_handler<Message>*>(h.get());
        mh->handle_message(msg, from);
      }
    }
  }

  // All messages associated with a single actor object should be processed
  // non-concurrently. We use a strand to ensure non-concurrent execution even
  // if the underlying executor may use multiple threads.
  strand<any_io_executor> executor_;

  std::vector<std::shared_ptr<message_handler_base>> handlers_;
};

// A concrete actor that allows synchronous message retrieval.
template <class Message>
class receiver : public actor
{
public:
  receiver()
    : actor(system_executor())
  {
    register_handler(&receiver::message_handler);
  }

  // Block until a message has been received.
  Message wait()
  {
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [this]{ return !message_queue_.empty(); });
    Message msg(std::move(message_queue_.front()));
    message_queue_.pop_front();
    return msg;
  }

private:
  // Handle a new message by adding it to the queue and waking a waiter.
  void message_handler(Message msg, actor_address /* from */)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    message_queue_.push_back(std::move(msg));
    condition_.notify_one();
  }

  std::mutex mutex_;
  std::condition_variable condition_;
  std::deque<Message> message_queue_;
};

//------------------------------------------------------------------------------

#include <boost/asio/thread_pool.hpp>
#include <iostream>

using boost::asio::thread_pool;

class member : public actor
{
public:
  explicit member(any_io_executor e)
    : actor(std::move(e))
  {
    register_handler(&member::init_handler);
  }

private:
  void init_handler(actor_address next, actor_address from)
  {
    next_ = next;
    caller_ = from;

    register_handler(&member::token_handler);
    deregister_handler(&member::init_handler);
  }

  void token_handler(int token, actor_address /*from*/)
  {
    int msg(token);
    actor_address to(caller_);

    if (token > 0)
    {
      msg = token - 1;
      to = next_;
    }

    tail_send(msg, to);
  }

  actor_address next_;
  actor_address caller_;
};

int main()
{
  const std::size_t num_threads = 16;
  const int num_hops = 50000000;
  const std::size_t num_actors = 503;
  const int token_value = (num_hops + num_actors - 1) / num_actors;
  const std::size_t actors_per_thread = num_actors / num_threads;

  struct single_thread_pool : thread_pool { single_thread_pool() : thread_pool(1) {} };
  single_thread_pool pools[num_threads];
  std::vector<std::shared_ptr<member>> members(num_actors);
  receiver<int> rcvr;

  // Create the member actors.
  for (std::size_t i = 0; i < num_actors; ++i)
    members[i] = std::make_shared<member>(pools[(i / actors_per_thread) % num_threads].get_executor());

  // Initialise the actors by passing each one the address of the next actor in the ring.
  for (std::size_t i = num_actors, next_i = 0; i > 0; next_i = --i)
    send(members[next_i]->address(), rcvr.address(), members[i - 1]->address());

  // Send exactly one token to each actor, all with the same initial value, rounding up if required.
  for (std::size_t i = 0; i < num_actors; ++i)
    send(token_value, rcvr.address(), members[i]->address());

  // Wait for all signal messages, indicating the tokens have all reached zero.
  for (std::size_t i = 0; i < num_actors; ++i)
    rcvr.wait();
}
