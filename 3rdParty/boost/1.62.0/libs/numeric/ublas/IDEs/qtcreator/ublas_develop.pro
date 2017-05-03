TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS = include benchmarks
OTHER_FILES += ../../changelog.txt

include (tests.pri)
