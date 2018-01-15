// Copyright (c) 2017 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)


#ifndef BOOST_PROCESS_DETAIL_POSIX_SIGCHLD_SERVICE_HPP_
#define BOOST_PROCESS_DETAIL_POSIX_SIGCHLD_SERVICE_HPP_

#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <boost/optional.hpp>
#include <signal.h>
#include <functional>
#include <sys/wait.h>

namespace boost { namespace process { namespace detail { namespace posix {

class sigchld_service : public boost::asio::detail::service_base<sigchld_service>
{
    boost::asio::io_context::strand _strand{get_io_context()};
    boost::asio::signal_set _signal_set{get_io_context(), SIGCHLD};

    std::vector<std::pair<::pid_t, std::function<void(int, std::error_code)>>> _receivers;
    inline void _handle_signal(const boost::system::error_code & ec);
public:
    sigchld_service(boost::asio::io_context & io_context)
        : boost::asio::detail::service_base<sigchld_service>(io_context)
    {
    }

    template <typename SignalHandler>
    BOOST_ASIO_INITFN_RESULT_TYPE(SignalHandler,
        void (int, std::error_code))
    async_wait(::pid_t pid, SignalHandler && handler)
    {
        boost::asio::async_completion<
        SignalHandler, void(boost::system::error_code)> init{handler};
        auto & h = init.completion_handler;
        _strand.post(
                [this, pid, h]
                {
                    if (_receivers.empty())
                        _signal_set.async_wait(
                                [this](const boost::system::error_code & ec, int)
                                {
                                    _strand.post([this,ec]{this->_handle_signal(ec);});
                                });
                    _receivers.emplace_back(pid, h);
                });

        return init.result.get();
    }
    void shutdown_service() override
    {
        _receivers.clear();
    }

    void cancel()
    {
        _signal_set.cancel();
    }
    void cancel(boost::system::error_code & ec)
    {
        _signal_set.cancel(ec);
    }
};


void sigchld_service::_handle_signal(const boost::system::error_code & ec)
{
    std::error_code ec_{ec.value(), std::system_category()};

    if (ec_)
    {
        for (auto & r : _receivers)
            r.second(-1, ec_);
        return;
    }
    int status;
    int pid = ::waitpid(0, &status, WNOHANG);

    auto itr = std::find_if(_receivers.begin(), _receivers.end(),
            [&pid](const std::pair<::pid_t, std::function<void(int, std::error_code)>> & p)
            {
                return p.first == pid;
            });
    if (itr != _receivers.cend())
    {
        _strand.get_io_context().wrap(itr->second)(status, ec_);
        _receivers.erase(itr);
    }
    if (!_receivers.empty())
    {
        _signal_set.async_wait(
            [this](const boost::system::error_code & ec, int)
            {
                _strand.post([ec]{});
                this->_handle_signal(ec);
            });
    }
}


}
}
}
}




#endif
