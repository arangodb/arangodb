TEMPLATE = app
TARGET = bench2

!include (../configuration.pri)

OTHER_FILES += \
    ../../../../benchmarks/bench2/Jamfile.v2

HEADERS += \
    ../../../../benchmarks/bench2/bench2.hpp

SOURCES += \
    ../../../../benchmarks/bench2/bench23.cpp \
    ../../../../benchmarks/bench2/bench22.cpp \
    ../../../../benchmarks/bench2/bench21.cpp \
    ../../../../benchmarks/bench2/bench2.cpp
