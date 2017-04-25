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
    
    TARGET=$1
    shift
    if test -z "$TARGET"; then
        ./scripts/build-deb.sh --buildDir build-docu --parallel 2

        # we expect this to be a symlink, so no -r ;-)
        rm -f build
        ln -s build-docu build
        
        ./utils/generateExamples.sh
    fi
    ./utils/generateSwagger.sh
    cd Documentation/Books
    if test -z "$TARGET"; then
        make build-dist-books OUTPUT_DIR=/build/build-docu NODE_MODLUES_DIR=/tmp/1/node_modules
    else
        make build-book NAME=$TARGET OUTPUT_DIR=/build/build-docu NODE_MODLUES_DIR=/tmp/1/node_modules $@
    fi
}

if test "$1" != "docker"; then

    cd "$(dirname "${BASH_SOURCE[0]}")/.."
    pwd

    N=`basename $0`
    if test "$N" != generateDocumentation.sh; then
        DOCTarget=`echo $N | sed "s;.sh;;"`
        cd ..
    fi
    WD=`pwd`

    docker pull arangodb/documentation-builder 
    docker run -it --volume ${WD}:/build arangodb/documentation-builder /bin/bash /build/scripts/generateDocumentation.sh docker $DOCTarget $@ 

else
    cd /build
    shift
    main "$@"
fi
