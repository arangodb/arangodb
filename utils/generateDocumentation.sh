#!/bin/bash

##python-setuptools
##
##python setup.py install
##
##node npm
##
##https://github.com/GitbookIO/gitbook
## npm install gitbook-cli -g
##
##  http://calibre-ebook.com/download

test_tools(){
    if ! type easy_install >> /dev/null; then
        echo "you are missing setuptools"
        echo "apt-get install python-setuptools"
        exit 1
    fi

    if ! type node >> /dev/null; then
        echo "you are missing node"
        echo "apt-get install nodejs nodejs-legacy"
        exit 1
    fi

    if ! type npm >> /dev/null; then
        echo "you are missing node"
        echo "apt-get install npm"
        exit 1
    fi

    if ! type calibre >> /dev/null; then
        echo "you are missing node"
        echo "apt-get install calibre-bin"
        exit 1
    fi
    ARANGODB_VERSION_MAJOR=`grep 'set(ARANGODB_VERSION_MAJOR' CMakeLists.txt | sed 's;.*"\(.*\)".*;\1;'`
    ARANGODB_VERSION_MINOR=`grep 'set(ARANGODB_VERSION_MINOR' CMakeLists.txt | sed 's;.*"\(.*\)".*;\1;'`
    ARANGODB_VERSION_REVISION=`grep 'set(ARANGODB_VERSION_REVISION' CMakeLists.txt | sed 's;.*"\(.*\)".*;\1;'`

}

main(){
    local source_dir="$(pwd)"
    #test for basic tools
    #test_tools

    #./utils/generateExamples.sh
    #./utils/generateSwagger.sh

    #INSTALLED_GITBOOK_VERSION=$(gitbook ls |grep '*'|sed "s;.*\* ;;")
    #if test -z "${INSTALLED_GITBOOK_VERSION}"; then
    #    echo "your container doesn't come with a preloaded version of gitbook, please update it."
    #    exit 1
    #fi
    #export GITBOOK_ARGS="--gitbook ${INSTALLED_GITBOOK_VERSION}"
    cd Documentation/Books

    ./build.sh build-dist-books --outputDir /build/build
}

main "$@"
