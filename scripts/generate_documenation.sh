#!/bin/bash

##python3-setuptools
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
    if ! type easy_install3 >> /dev/null; then
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

install_tools(){
    (
        if ! [[ -f markdown-pp ]]; then
            git clone https://github.com/arangodb-helper/markdown-pp/
        fi
        cd  markdown-pp
        python2 setup.py install --user
    )
    npm install gitbook-cli


}

main(){
    #test for basic tools
    test_tools

    #cd into target dir
    mkdir -p "$1"
    cd $1 || { echo "unable to change into $1"; exit 1; }

    install_tools

}

main "$@"

