#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources(include/boost/beast beast)
GroupSources(example/cppcon2018 "/")

file (GLOB APP_FILES
  beast.hpp
  http_session.cpp
  http_session.hpp
  Jamfile
  listener.cpp
  listener.hpp
  main.cpp
  net.hpp
  shared_state.cpp
  shared_state.hpp
  websocket_session.cpp
  websocket_session.hpp
  chat_client.html
  README.md
)

source_group ("" FILES ${APP_FILES})

add_executable (websocket-chat-server
    ${APP_FILES}
    ${BOOST_BEAST_FILES}
)

set_property(TARGET websocket-chat-server PROPERTY FOLDER "example-cppcon2018")
