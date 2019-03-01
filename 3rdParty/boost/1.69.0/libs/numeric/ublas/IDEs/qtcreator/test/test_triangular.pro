TEMPLATE = app
TARGET = test_triangular

!include (configuration.pri)

DEFINES += \
    BOOST_CHRONO_DYN_LINK=1 \
    BOOST_CHRONO_THREAD_DISABLED \
    BOOST_SYSTEM_DYN_LINK=1 \
    BOOST_SYSTEM_NO_DEPRECATED \
    BOOST_TIMER_DYN_LINK=1

SOURCES += \
    ../../../test/test_triangular.cpp

LIBS += -lboost_timer -lboost_system -lboost_chrono
