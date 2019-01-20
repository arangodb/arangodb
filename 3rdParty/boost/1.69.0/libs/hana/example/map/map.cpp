// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.

#include <boost/hana/assert.hpp>
#include <boost/hana/at_key.hpp>
#include <boost/hana/contains.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/string.hpp>

#include <functional>
#include <string>
#include <vector>
namespace hana = boost::hana;


template <typename ...Events>
struct event_system {
    using Callback = std::function<void()>;
    hana::map<hana::pair<Events, std::vector<Callback>>...> map_;

    template <typename Event, typename F>
    void on(Event e, F handler) {
        auto is_known_event = hana::contains(map_, e);
        static_assert(is_known_event,
            "trying to add a handler to an unknown event");

        map_[e].push_back(handler);
    }

    template <typename Event>
    void trigger(Event e) const {
        auto is_known_event = hana::contains(map_, e);
        static_assert(is_known_event,
            "trying to trigger an unknown event");

        for (auto& handler : this->map_[e])
            handler();
    }
};

template <typename ...Events>
event_system<Events...> make_event_system(Events ...events) {
    return {};
}


int main() {
    auto events = make_event_system(
        BOOST_HANA_STRING("foo"),
        BOOST_HANA_STRING("bar"),
        BOOST_HANA_STRING("baz")
    );

    std::vector<std::string> triggered_events;
    events.on(BOOST_HANA_STRING("foo"), [&] {
        triggered_events.push_back("foo:1");
    });
    events.on(BOOST_HANA_STRING("foo"), [&] {
        triggered_events.push_back("foo:2");
    });
    events.on(BOOST_HANA_STRING("bar"), [&] {
        triggered_events.push_back("bar:1");
    });
    events.on(BOOST_HANA_STRING("baz"), [&] {
        triggered_events.push_back("baz:1");
    });

    events.trigger(BOOST_HANA_STRING("foo"));
    events.trigger(BOOST_HANA_STRING("bar"));
    events.trigger(BOOST_HANA_STRING("baz"));

    BOOST_HANA_RUNTIME_CHECK(triggered_events == std::vector<std::string>{
        "foo:1", "foo:2", "bar:1", "baz:1"
    });
}
