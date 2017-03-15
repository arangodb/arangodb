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

    ./scripts/build-deb.sh --buildDir build-docu

    # we expect this to be a symlink, so no -r ;-)
    rm -f build
    ln -s build-docu build
    
    ./utils/generateExamples.sh
    ./utils/generateSwagger.sh
    cd Documentation/Books
    make build-dist-books
}

if test "$1" != "docker"; then
    cd "$(dirname "${BASH_SOURCE[0]}")/.."
    pwd
    WD=`pwd`
    docker run -it --volume ${WD}:/build arangodb/documentation-builder /build/scripts/generateDocumentation.sh docker $@

else
    cd /build
    main "$@"
fi
