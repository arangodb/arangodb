//          Copyright Oliver Kowalke 2015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstddef>
#include <cstdlib>
#include <map>
#include <memory>
#include <set>
#include <iostream>
#include <string>

#include <boost/asio.hpp>
#include <boost/utility.hpp>

#include <boost/fiber/all.hpp>
#include "../round_robin.hpp"
#include "../yield.hpp"

using boost::asio::ip::tcp;

const std::size_t max_length = 1024;

class subscriber_session;
typedef std::shared_ptr< subscriber_session > subscriber_session_ptr;

// a queue has n subscribers (subscriptions)
// this class holds a list of subcribers for one queue
class subscriptions {
public:
    ~subscriptions();

    // subscribe to this queue
    void subscribe( subscriber_session_ptr const& s) {
        subscribers_.insert( s);
    }

    // unsubscribe from this queue
    void unsubscribe( subscriber_session_ptr const& s) {
        subscribers_.erase(s);
    }

    // publish a message, e.g. push this message to all subscribers
    void publish( std::string const& msg);

private:
    // list of subscribers
    std::set< subscriber_session_ptr >  subscribers_;
};

// a class to register queues and to subsribe clients to this queues
class registry : private boost::noncopyable {
private:
    typedef std::map< std::string, std::shared_ptr< subscriptions > > queues_cont;
    typedef queues_cont::iterator queues_iter;

    boost::fibers::mutex    mtx_;
    queues_cont           queues_;

    void register_queue_( std::string const& queue) {
        if ( queues_.end() != queues_.find( queue) ) {
            throw std::runtime_error("queue already exists");
        }
        queues_[queue] = std::make_shared< subscriptions >();
        std::cout << "new queue '" << queue << "' registered" << std::endl;
    }

    void unregister_queue_( std::string const& queue) {
        queues_.erase( queue);
        std::cout << "queue '" << queue << "' unregistered" << std::endl;
    }

    void subscribe_( std::string const& queue, subscriber_session_ptr s) {
        queues_iter iter = queues_.find( queue);
        if ( queues_.end() == iter ) {
            throw std::runtime_error("queue does not exist");
        }
        iter->second->subscribe( s);
        std::cout << "new subscription to queue '" << queue << "'" << std::endl;
    }

    void unsubscribe_( std::string const& queue, subscriber_session_ptr s) {
        queues_iter iter = queues_.find( queue);
        if ( queues_.end() != iter ) {
            iter->second->unsubscribe( s);
        }
    }

    void publish_( std::string const& queue, std::string const& msg) {
        queues_iter iter = queues_.find( queue);
        if ( queues_.end() == iter ) {
            throw std::runtime_error("queue does not exist");
        }
        iter->second->publish( msg);
        std::cout << "message '" << msg << "' to publish on queue '" << queue << "'" << std::endl;
    }

public:
    // add a queue to registry
    void register_queue( std::string const& queue) {
        std::unique_lock< boost::fibers::mutex > lk( mtx_);
        register_queue_( queue);
    }

    // remove a queue from registry
    void unregister_queue( std::string const& queue) {
        std::unique_lock< boost::fibers::mutex > lk( mtx_);
        unregister_queue_( queue);
    }

    // subscribe to a queue
    void subscribe( std::string const& queue, subscriber_session_ptr s) {
        std::unique_lock< boost::fibers::mutex > lk( mtx_);
        subscribe_( queue, s);
    }

    // unsubscribe from a queue
    void unsubscribe( std::string const& queue, subscriber_session_ptr s) {
        std::unique_lock< boost::fibers::mutex > lk( mtx_);
        unsubscribe_( queue, s);
    }

    // publish a message to all subscribers registerd to the queue
    void publish( std::string const& queue, std::string const& msg) {
        std::unique_lock< boost::fibers::mutex > lk( mtx_);
        publish_( queue, msg);
    }
};

// a subscriber subscribes to a given queue in order to receive messages published on this queue
class subscriber_session : public std::enable_shared_from_this< subscriber_session > {
public:
    explicit subscriber_session( std::shared_ptr< boost::asio::io_service > const& io_service, registry & reg) :
        socket_( * io_service),
        reg_( reg) {
    }

    tcp::socket& socket() {
        return socket_;
    }

    // this function is executed inside the fiber
    void run() {
        std::string queue;
        try {
            boost::system::error_code ec;
            // read first message == queue name
            // async_ready() returns if the the complete message is read
            // until this the fiber is suspended until the complete message
            // is read int the given buffer 'data'
            boost::asio::async_read(
                    socket_,
                    boost::asio::buffer( data_),
                    boost::fibers::asio::yield[ec]);
            if ( ec) {
                throw std::runtime_error("no queue from subscriber");
            }
            // first message ist equal to the queue name the publisher
            // publishes to
            queue = data_;
            // subscribe to new queue
            reg_.subscribe( queue, shared_from_this() );
            // read published messages
            for (;;) {
                // wait for a conditon-variable for new messages
                // the fiber will be suspended until the condtion
                // becomes true and the fiber is resumed
                // published message is stored in buffer 'data_'
                std::unique_lock< boost::fibers::mutex > lk( mtx_);
                cond_.wait( lk);
                std::string data( data_);
                lk.unlock();
                // message '<fini>' terminates subscription
                if ( "<fini>" == data) {
                    break;
                }
                // async. write message to socket connected with
                // subscriber
                // async_write() returns if the complete message was writen
                // the fiber is suspended in the meanwhile
                boost::asio::async_write(
                        socket_,
                        boost::asio::buffer( data, data.size() ),
                        boost::fibers::asio::yield[ec]);
                if ( ec == boost::asio::error::eof) {
                    break; //connection closed cleanly by peer
                } else if ( ec) {
                    throw boost::system::system_error( ec); //some other error
                }
                std::cout << "subscriber::run(): '" << data << "' written" << std::endl;
            }
        } catch ( std::exception const& e) {
            std::cerr << "subscriber [" << queue << "] failed: " << e.what() << std::endl;
        }
        // close socket
        socket_.close();
        // unregister queue
        reg_.unsubscribe( queue, shared_from_this() );
    }

    // called from publisher_session (running in other fiber)
    void publish( std::string const& msg) {
        std::unique_lock< boost::fibers::mutex > lk( mtx_);
        std::memset( data_, '\0', sizeof( data_));
        std::memcpy( data_, msg.c_str(), (std::min)(max_length, msg.size()));
        cond_.notify_one();
    }

private:
    tcp::socket                         socket_;
    registry                        &   reg_;
    boost::fibers::mutex                mtx_;
    boost::fibers::condition_variable   cond_;
    // fixed size message
    char                                data_[max_length];
};


subscriptions::~subscriptions() {
    for ( subscriber_session_ptr s : subscribers_) {
        s->publish("<fini>");
    } 
}

void
subscriptions::publish( std::string const& msg) {
    for ( subscriber_session_ptr s : subscribers_) {
        s->publish( msg);
    }
}

// a publisher publishes messages on its queue
// subscriber might register to this queue to get the published messages
class publisher_session : public std::enable_shared_from_this< publisher_session > {
public:
    explicit publisher_session( std::shared_ptr< boost::asio::io_service > const& io_service, registry & reg) :
        socket_( * io_service),
        reg_( reg) {
    }

    tcp::socket& socket() {
        return socket_;
    }

    // this function is executed inside the fiber
    void run() {
        std::string queue;
        try {
            boost::system::error_code ec;
            // fixed size message
            char data[max_length];
            // read first message == queue name
            // async_ready() returns if the the complete message is read
            // until this the fiber is suspended until the complete message
            // is read int the given buffer 'data'
            boost::asio::async_read(
                    socket_,
                    boost::asio::buffer( data),
                    boost::fibers::asio::yield[ec]);
            if ( ec) {
                throw std::runtime_error("no queue from publisher");
            }
            // first message ist equal to the queue name the publisher
            // publishes to
            queue = data;
            // register the new queue
            reg_.register_queue( queue);
            // start publishing messages
            for (;;) {
                // read message from publisher asyncronous
                // async_read() suspends this fiber until the complete emssage is read
                // and stored in the given buffer 'data'
                boost::asio::async_read(
                        socket_,
                        boost::asio::buffer( data),
                        boost::fibers::asio::yield[ec]); 
                if ( ec == boost::asio::error::eof) {
                    break; //connection closed cleanly by peer
                } else if ( ec) {
                    throw boost::system::system_error( ec); //some other error
                }
                // publish message to all subscribers
                reg_.publish( queue, std::string( data) );
            }
        } catch ( std::exception const& e) {
            std::cerr << "publisher [" << queue << "] failed: " << e.what() << std::endl;
        }
        // close socket
        socket_.close();
        // unregister queue
        reg_.unregister_queue( queue);
    }

private:
    tcp::socket         socket_;
    registry        &   reg_;
};

typedef std::shared_ptr< publisher_session > publisher_session_ptr;

// function accepts connections requests from clients acting as a publisher
void accept_publisher( std::shared_ptr< boost::asio::io_service > const& io_service,
                       unsigned short port,
                       registry & reg) {
    // create TCP-acceptor
    tcp::acceptor acceptor( * io_service, tcp::endpoint( tcp::v4(), port) );
    // loop for accepting connection requests
    for (;;) {
        boost::system::error_code ec;
        // create new publisher-session
        // this instance will be associated with one publisher
        publisher_session_ptr new_publisher_session = 
            std::make_shared< publisher_session >( io_service, std::ref( reg) );
        // async. accept of new connection request
        // this function will suspend this execution context (fiber) until a
        // connection was established, after returning from this function a new client (publisher)
        // is connected
        acceptor.async_accept(
                new_publisher_session->socket(), 
                boost::fibers::asio::yield[ec]);
        if ( ! ec) {
            // run the new publisher in its own fiber (one fiber for one client)
            boost::fibers::fiber(
                std::bind( & publisher_session::run, new_publisher_session) ).detach();
        }
    }
}

// function accepts connections requests from clients acting as a subscriber
void accept_subscriber( std::shared_ptr< boost::asio::io_service > const& io_service,
                        unsigned short port,
                        registry & reg) {
    // create TCP-acceptor
    tcp::acceptor acceptor( * io_service, tcp::endpoint( tcp::v4(), port) );
    // loop for accepting connection requests
    for (;;) {
        boost::system::error_code ec;
        // create new subscriber-session
        // this instance will be associated with one subscriber
        subscriber_session_ptr new_subscriber_session = 
            std::make_shared< subscriber_session >( io_service, std::ref( reg) );
        // async. accept of new connection request
        // this function will suspend this execution context (fiber) until a
        // connection was established, after returning from this function a new client (subscriber)
        // is connected
        acceptor.async_accept(
                new_subscriber_session->socket(), 
                boost::fibers::asio::yield[ec]);
        if ( ! ec) {
            // run the new subscriber in its own fiber (one fiber for one client)
            boost::fibers::fiber(
                std::bind( & subscriber_session::run, new_subscriber_session) ).detach();
        }
    }
}


int main( int argc, char* argv[]) {
    try {
        // create io_service for async. I/O
        std::shared_ptr< boost::asio::io_service > io_service = std::make_shared< boost::asio::io_service >();
        // register asio scheduler
        boost::fibers::use_scheduling_algorithm< boost::fibers::asio::round_robin >( io_service);
        // registry for queues and its subscription
        registry reg;
        // create an acceptor for publishers, run it as fiber
        boost::fibers::fiber(
            accept_publisher, std::ref( io_service), 9997, std::ref( reg) ).detach();
        // create an acceptor for subscribers, run it as fiber
        boost::fibers::fiber(
            accept_subscriber, std::ref( io_service), 9998, std::ref( reg) ).detach();
        // dispatch
        io_service->run();
        return EXIT_SUCCESS;
    } catch ( std::exception const& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return EXIT_FAILURE;
}
