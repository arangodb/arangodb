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
}

main(){
    #test for basic tools
    test_tools

    ./scripts/build-deb.sh

    ln -s build-deb build
    
    ./utils/generateExamples.sh
    ./utils/generateSwagger.sh
    cd Documentation/Books
    make build-dist-books
}

main "$@"

