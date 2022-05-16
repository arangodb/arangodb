CONFIG -= qt
CONFIG += depend_includepath
win*: CONFIG += console

# ublas include directory
INCLUDEPATH += \
	../../../../../include

QMAKE_CXXFLAGS += -fno-inline
QMAKE_CXXFLAGS += -std=c++17

# If ublas tests are build with boost source code then,
# then boost headers and boost libraries should be used.
exists(../../../../../../boost-build.jam) {
    INCLUDEPATH += ../../../../../../..
    LIBS += -L../../../../../../../stage/lib
    QMAKE_RPATHDIR += ../../../../../../../stage/lib
}
