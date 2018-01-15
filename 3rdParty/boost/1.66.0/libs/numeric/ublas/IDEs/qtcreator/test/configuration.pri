CONFIG -= qt
CONFIG += \
    depend_includepath \
    debug
QMAKE_CXXFLAGS += -fno-inline

# Create a directory for each test.
DESTDIR = $${TARGET}
OBJECTS_DIR = $${TARGET}

UBLAS_TESTSET = \
    USE_DOUBLE USE_STD_COMPLEX \
    USE_RANGE USE_SLICE \
    USE_UNBOUNDED_ARRAY USE_STD_VECTOR USE_BOUNDED_VECTOR USE_MATRIX

UBLAS_TESTSET_SPARSE = \
    USE_DOUBLE USE_STD_COMPLEX \
    USE_UNBOUNDED_ARRAY \
    USE_MAP_ARRAY USE_STD_MAP \
    USE_MAPPED_VECTOR USE_COMPRESSED_VECTOR \
    USE_MAPPED_MATRIX USE_COMPRESSED_MATRIX
    # USE_RANGE USE_SLICE        # Too complex for regression testing

UBLAS_TESTSET_SPARSE_COO = \
    USE_DOUBLE USE_STD_COMPLEX \
    USE_UNBOUNDED_ARRAY \
    USE_COORDINATE_VECTOR \
    USE_COORDINATE_MATRIX

DEFINES += BOOST_UBLAS_NO_EXCEPTIONS

#Visual age IBM
xlc: DEFINES += BOOST_UBLAS_NO_ELEMENT_PROXIES

# ublas include and test directory are included
INCLUDEPATH += \
    ../../../include \
    ../../test

# If ublas tests are build with boost source code then,
# then boost headers and boost libraries should be used.
exists(../../../../../../boost-build.jam) {
    INCLUDEPATH += ../../../../../..
    LIBS += -L../../../../../../stage/lib
    QMAKE_RPATHDIR += ../../../../../../stage/lib
}

# Execute test once compiled.
win32: QMAKE_POST_LINK = ./$${DESTDIR}/$${TARGET}.exe
else: QMAKE_POST_LINK = ./$${DESTDIR}/$${TARGET}

