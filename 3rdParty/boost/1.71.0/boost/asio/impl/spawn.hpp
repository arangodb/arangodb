//
// impl/spawn.hpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2019 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_IMPL_SPAWN_HPP
#define BOOST_ASIO_IMPL_SPAWN_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/detail/config.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/detail/atomic_count.hpp>
#include <boost/asio/detail/handler_alloc_helpers.hpp>
#include <boost/asio/detail/handler_cont_helpers.hpp>
#include <boost/asio/detail/handler_invoke_helpers.hpp>
#include <boost/asio/detail/memory.hpp>
#include <boost/asio/detail/noncopyable.hpp>
#include <boost/asio/detail/type_traits.hpp>
#include <boost/system/system_error.hpp>

#include <boost/asio/detail/push_options.hpp>

namespace boost {
namespace asio {
namespace detail {

  template <typename Handler, typename T>
  class coro_handler
  {
  public:
    coro_handler(basic_yield_context<Handler> ctx)
      : coro_(ctx.coro_.lock()),
        ca_(ctx.ca_),
        handler_(ctx.handler_),
        ready_(0),
        ec_(ctx.ec_),
        value_(0)
    {
    }

    void operator()(T value)
    {
      *ec_ = boost::system::error_code();
      *value_ = BOOST_ASIO_MOVE_CAST(T)(value);
      if (--*ready_ == 0)
        (*coro_)();
    }

    void operator()(boost::system::error_code ec, T value)
    {
      *ec_ = ec;
      *value_ = BOOST_ASIO_MOVE_CAST(T)(value);
      if (--*ready_ == 0)
        (*coro_)();
    }

  //private:
    shared_ptr<typename basic_yield_context<Handler>::callee_type> coro_;
    typename basic_yield_context<Handler>::caller_type& ca_;
    Handler handler_;
    atomic_count* ready_;
    boost::system::error_code* ec_;
    T* value_;
  };

  template <typename Handler>
  class coro_handler<Handler, void>
  {
  public:
    coro_handler(basic_yield_context<Handler> ctx)
      : coro_(ctx.coro_.lock()),
        ca_(ctx.ca_),
        handler_(ctx.handler_),
        ready_(0),
        ec_(ctx.ec_)
    {
    }

    void operator()()
    {
      *ec_ = boost::system::error_code();
      if (--*ready_ == 0)
        (*coro_)();
    }

    void operator()(boost::system::error_code ec)
    {
      *ec_ = ec;
      if (--*ready_ == 0)
        (*coro_)();
    }

  //private:
    shared_ptr<typename basic_yield_context<Handler>::callee_type> coro_;
    typename basic_yield_context<Handler>::caller_type& ca_;
    Handler handler_;
    atomic_count* ready_;
    boost::system::error_code* ec_;
  };

  template <typename Handler, typename T>
  inline void* asio_handler_allocate(std::size_t size,
      coro_handler<Handler, T>* this_handler)
  {
    return boost_asio_handler_alloc_helpers::allocate(
        size, this_handler->handler_);
  }

  template <typename Handler, typename T>
  inline void asio_handler_deallocate(void* pointer, std::size_t size,
      coro_handler<Handler, T>* this_handler)
  {
    boost_asio_handler_alloc_helpers::deallocate(
        pointer, size, this_handler->handler_);
  }

  template <typename Handler, typename T>
  inline bool asio_handler_is_continuation(coro_handler<Handler, T>*)
  {
    return true;
  }

  template <typename Function, typename Handler, typename T>
  inline void asio_handler_invoke(Function& function,
      coro_handler<Handler, T>* this_handler)
  {
    boost_asio_handler_invoke_helpers::invoke(
        function, this_handler->handler_);
  }

  template <typename Function, typename Handler, typename T>
  inline void asio_handler_invoke(const Function& function,
      coro_handler<Handler, T>* this_handler)
  {
    boost_asio_handler_invoke_helpers::invoke(
        function, this_handler->handler_);
  }

  template <typename Handler, typename T>
  class coro_async_result
  {
  public:
    typedef coro_handler<Handler, T> completion_handler_type;
    typedef T return_type;

    explicit coro_async_result(completion_handler_type& h)
      : handler_(h),
        ca_(h.ca_),
        ready_(2)
    {
      h.ready_ = &ready_;
      out_ec_ = h.ec_;
      if (!out_ec_) h.ec_ = &ec_;
      h.value_ = &value_;
    }

    return_type get()
    {
      // Must not hold shared_ptr to coro while suspended.
      handler_.coro_.reset();

      if (--ready_ != 0)
        ca_();
      if (!out_ec_ && ec_) throw boost::system::system_error(ec_);
      return BOOST_ASIO_MOVE_CAST(return_type)(value_);
    }

  private:
    completion_handler_type& handler_;
    typename basic_yield_context<Handler>::caller_type& ca_;
    atomic_count ready_;
    boost::system::error_code* out_ec_;
    boost::system::error_code ec_;
    return_type value_;
  };

  template <typename Handler>
  class coro_async_result<Handler, void>
  {
  public:
    typedef coro_handler<Handler, void> completion_handler_type;
    typedef void return_type;

    explicit coro_async_result(completion_handler_type& h)
      : handler_(h),
        ca_(h.ca_),
        ready_(2)
    {
      h.ready_ = &ready_;
      out_ec_ = h.ec_;
      if (!out_ec_) h.ec_ = &ec_;
    }

    void get()
    {
      // Must not hold shared_ptr to coro while suspended.
      handler_.coro_.reset();

      if (--ready_ != 0)
        ca_();
      if (!out_ec_ && ec_) throw boost::system::system_error(ec_);
    }

  private:
    completion_handler_type& handler_;
    typename basic_yield_context<Handler>::caller_type& ca_;
    atomic_count ready_;
    boost::system::error_code* out_ec_;
    boost::system::error_code ec_;
  };

} // namespace detail

#if !defined(GENERATING_DOCUMENTATION)

template <typename Handler, typename ReturnType>
class async_result<basic_yield_context<Handler>, ReturnType()>
  : public detail::coro_async_result<Handler, void>
{
public:
  explicit async_result(
    typename detail::coro_async_result<Handler,
      void>::completion_handler_type& h)
    : detail::coro_async_result<Handler, void>(h)
  {
  }
};

template <typename Handler, typename ReturnType, typename Arg1>
class async_result<basic_yield_context<Handler>, ReturnType(Arg1)>
  : public detail::coro_async_result<Handler, typename decay<Arg1>::type>
{
public:
  explicit async_result(
    typename detail::coro_async_result<Handler,
      typename decay<Arg1>::type>::completion_handler_type& h)
    : detail::coro_async_result<Handler, typename decay<Arg1>::type>(h)
  {
  }
};

template <typename Handler, typename ReturnType>
class async_result<basic_yield_context<Handler>,
    ReturnType(boost::system::error_code)>
  : public detail::coro_async_result<Handler, void>
{
public:
  explicit async_result(
    typename detail::coro_async_result<Handler,
      void>::completion_handler_type& h)
    : detail::coro_async_result<Handler, void>(h)
  {
  }
};

template <typename Handler, typename ReturnType, typename Arg2>
class async_result<basic_yield_context<Handler>,
    ReturnType(boost::system::error_code, Arg2)>
  : public detail::coro_async_result<Handler, typename decay<Arg2>::type>
{
public:
  explicit async_result(
    typename detail::coro_async_result<Handler,
      typename decay<Arg2>::type>::completion_handler_type& h)
    : detail::coro_async_result<Handler, typename decay<Arg2>::type>(h)
  {
  }
};

template <typename Handler, typename T, typename Allocator>
struct associated_allocator<detail::coro_handler<Handler, T>, Allocator>
{
  typedef typename associated_allocator<Handler, Allocator>::type type;

  static type get(const detail::coro_handler<Handler, T>& h,
      const Allocator& a = Allocator()) BOOST_ASIO_NOEXCEPT
  {
    return associated_allocator<Handler, Allocator>::get(h.handler_, a);
  }
};

template <typename Handler, typename T, typename Executor>
struct associated_executor<detail::coro_handler<Handler, T>, Executor>
{
  typedef typename associated_executor<Handler, Executor>::type type;

  static type get(const detail::coro_handler<Handler, T>& h,
      const Executor& ex = Executor()) BOOST_ASIO_NOEXCEPT
  {
    return associated_executor<Handler, Executor>::get(h.handler_, ex);
  }
};

namespace detail {

  template <typename Handler, typename Function>
  struct spawn_data : private noncopyable
  {
    template <typename Hand, typename Func>
    spawn_data(BOOST_ASIO_MOVE_ARG(Hand) handler,
        bool call_handler, BOOST_ASIO_MOVE_ARG(Func) function)
      : handler_(BOOST_ASIO_MOVE_CAST(Hand)(handler)),
        call_handler_(call_handler),
        function_(BOOST_ASIO_MOVE_CAST(Func)(function))
    {
    }

    weak_ptr<typename basic_yield_context<Handler>::callee_type> coro_;
    Handler handler_;
    bool call_handler_;
    Function function_;
  };

  template <typename Handler, typename Function>
  struct coro_entry_point
  {
    void operator()(typename basic_yield_context<Handler>::caller_type& ca)
    {
      shared_ptr<spawn_data<Handler, Function> > data(data_);
#if !defined(BOOST_COROUTINES_UNIDIRECT) && !defined(BOOST_COROUTINES_V2)
      ca(); // Yield until coroutine pointer has been initialised.
#endif // !defined(BOOST_COROUTINES_UNIDIRECT) && !defined(BOOST_COROUTINES_V2)
      const basic_yield_context<Handler> yield(
          data->coro_, ca, data->handler_);

      (data->function_)(yield);
      if (data->call_handler_)
        (data->handler_)();
    }

    shared_ptr<spawn_data<Handler, Function> > data_;
  };

  template <typename Handler, typename Function>
  struct spawn_helper
  {
    void operator()()
    {
      typedef typename basic_yield_context<Handler>::callee_type callee_type;
      coro_entry_point<Handler, Function> entry_point = { data_ };
      shared_ptr<callee_type> coro(new callee_type(entry_point, attributes_));
      data_->coro_ = coro;
      (*coro)();
    }

    shared_ptr<spawn_data<Handler, Function> > data_;
    boost::coroutines::attributes attributes_;
  };

  template <typename Function, typename Handler, typename Function1>
  inline void asio_handler_invoke(Function& function,
      spawn_helper<Handler, Function1>* this_handler)
  {
    boost_asio_handler_invoke_helpers::invoke(
        function, this_handler->data_->handler_);
  }

  template <typename Function, typename Handler, typename Function1>
  inline void asio_handler_invoke(const Function& function,
      spawn_helper<Handler, Function1>* this_handler)
  {
    boost_asio_handler_invoke_helpers::invoke(
        function, this_handler->data_->handler_);
  }

  inline void default_spawn_handler() {}

} // namespace detail

template <typename Function>
inline void spawn(BOOST_ASIO_MOVE_ARG(Function) function,
    const boost::coroutines::attributes& attributes)
{
  typedef typename decay<Function>::type function_type;

  typename associated_executor<function_type>::type ex(
      (get_associated_executor)(function));

  boost::asio::spawn(ex, BOOST_ASIO_MOVE_CAST(Function)(function), attributes);
}

template <typename Handler, typename Function>
void spawn(BOOST_ASIO_MOVE_ARG(Handler) handler,
    BOOST_ASIO_MOVE_ARG(Function) function,
    const boost::coroutines::attributes& attributes,
    typename enable_if<!is_executor<typename decay<Handler>::type>::value &&
      !is_convertible<Handler&, execution_context&>::value>::type*)
{
  typedef typename decay<Handler>::type handler_type;
  typedef typename decay<Function>::type function_type;

  typename associated_executor<handler_type>::type ex(
      (get_associated_executor)(handler));

  typename associated_allocator<handler_type>::type a(
      (get_associated_allocator)(handler));

  detail::spawn_helper<handler_type, function_type> helper;
  helper.data_.reset(
      new detail::spawn_data<handler_type, function_type>(
        BOOST_ASIO_MOVE_CAST(Handler)(handler), true,
        BOOST_ASIO_MOVE_CAST(Function)(function)));
  helper.attributes_ = attributes;

  ex.dispatch(helper, a);
}

template <typename Handler, typename Function>
void spawn(basic_yield_context<Handler> ctx,
    BOOST_ASIO_MOVE_ARG(Function) function,
    const boost::coroutines::attributes& attributes)
{
  typedef typename decay<Function>::type function_type;

  Handler handler(ctx.handler_); // Explicit copy that might be moved from.

  typename associated_executor<Handler>::type ex(
      (get_associated_executor)(handler));

  typename associated_allocator<Handler>::type a(
      (get_associated_allocator)(handler));

  detail::spawn_helper<Handler, function_type> helper;
  helper.data_.reset(
      new detail::spawn_data<Handler, function_type>(
        BOOST_ASIO_MOVE_CAST(Handler)(handler), false,
        BOOST_ASIO_MOVE_CAST(Function)(function)));
  helper.attributes_ = attributes;

  ex.dispatch(helper, a);
}

template <typename Function, typename Executor>
inline void spawn(const Executor& ex,
    BOOST_ASIO_MOVE_ARG(Function) function,
    const boost::coroutines::attributes& attributes,
    typename enable_if<is_executor<Executor>::value>::type*)
{
  boost::asio::spawn(boost::asio::strand<Executor>(ex),
      BOOST_ASIO_MOVE_CAST(Function)(function), attributes);
}

template <typename Function, typename Executor>
inline void spawn(const strand<Executor>& ex,
    BOOST_ASIO_MOVE_ARG(Function) function,
    const boost::coroutines::attributes& attributes)
{
  boost::asio::spawn(boost::asio::bind_executor(
        ex, &detail::default_spawn_handler),
      BOOST_ASIO_MOVE_CAST(Function)(function), attributes);
}

template <typename Function>
inline void spawn(const boost::asio::io_context::strand& s,
    BOOST_ASIO_MOVE_ARG(Function) function,
    const boost::coroutines::attributes& attributes)
{
  boost::asio::spawn(boost::asio::bind_executor(
        s, &detail::default_spawn_handler),
      BOOST_ASIO_MOVE_CAST(Function)(function), attributes);
}

template <typename Function, typename ExecutionContext>
inline void spawn(ExecutionContext& ctx,
    BOOST_ASIO_MOVE_ARG(Function) function,
    const boost::coroutines::attributes& attributes,
    typename enable_if<is_convertible<
      ExecutionContext&, execution_context&>::value>::type*)
{
  boost::asio::spawn(ctx.get_executor(),
      BOOST_ASIO_MOVE_CAST(Function)(function), attributes);
}

#endif // !defined(GENERATING_DOCUMENTATION)

} // namespace asio
} // namespace boost

#include <boost/asio/detail/pop_options.hpp>

#endif // BOOST_ASIO_IMPL_SPAWN_HPP
