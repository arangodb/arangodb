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

    if test "${ARANGODB_VERSION_REVISION}" == "devel"; then
        export NODE_MODULES_DIR="/tmp/devel/node_modules"
    else
        export NODE_MODULES_DIR="/tmp/${ARANGODB_VERSION_MAJOR}.${ARANGODB_VERSION_MINOR}/node_modules"
    fi

    if test ! -d ${NODE_MODULES_DIR}; then
        echo "Your docker container doesn't contain the needed modules to build the documentation: ${NODE_MODULES_DIR}"
        echo "Please delete the old arangodb/documentation-builder container, and re-run this script so it will pull "
        echo "the latest version."
        exit 1
    fi
    if test $(whoami) != "root"; then
        cp -a /root/.gitbook/ ~/
        cp -a /root/.npm/ ~/
    fi
}

main(){
    #test for basic tools
    test_tools

    TARGET=$1
    shift
    if test -z "$TARGET"; then
        if test -d enterprise; then
            ENTERPRISE="--enterprise true"
        fi
        ./scripts/build-deb.sh --buildDir build-docu --parallel $(nproc) ${ENTERPRISE} ||exit 1

        # we expect this to be a symlink, so no -r ;-)
        echo "#############################################"
        echo "RELINKING BUILD DIRECTORY !!!!!!!!!!!!!!!!!!!"
        echo "#############################################"
        rm -f build
        ln -s build-docu build

        ./utils/generateExamples.sh
    fi
    ./utils/generateSwagger.sh

    INSTALLED_GITBOOK_VERSION=$(gitbook ls |grep '*'|sed "s;.*\* ;;")
    if test -z "${INSTALLED_GITBOOK_VERSION}"; then
        echo "your container doesn't come with a preloaded version of gitbook, please update it."
        exit 1
    fi
    export GITBOOK_ARGS="--gitbook ${INSTALLED_GITBOOK_VERSION}"

    cd Documentation/Books

    if test -z "$TARGET"; then
        ./build.sh build-dist-books --outputDir /build/build-docu --nodeModulesDir "${NODE_MODULES_DIR}"
    else
        ./build.sh build-book --name "$TARGET" --outputDir /build/build-docu  --nodeModulesDir "${NODE_MODULES_DIR}" $@
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
