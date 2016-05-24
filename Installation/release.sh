#!/bin/bash
set -e

TAG=1
BOOK=1

if [ "$1" == "--no-commit" ];  then
  TAG=0
  shift
fi

if [ "$1" == "--no-book" ];  then
  BOOK=0
  shift
fi

if [ "$#" -ne 1 ];  then
  echo "usage: $0 <major>.<minor>.<revision>"
  exit 1
fi

VERSION="$1"

if git tag | grep -q "^v$VERSION$";  then
  echo "$0: version $VERSION already defined"
  exit 1
fi

if fgrep -q "v$VERSION" CHANGELOG;  then
 echo "version $VERSION defined in CHANGELOG" 
else
  echo "$0: version $VERSION not defined in CHANGELOG"
  exit 1
fi

VERSION_MAJOR=`echo $VERSION | awk -F. '{print $1}'`
VERSION_MINOR=`echo $VERSION | awk -F. '{print $2}'`
VERSION_REVISION=`echo $VERSION | awk -F. '{print $3}'`

cat CMakeLists.txt \
  | sed -e "s~set(ARANGODB_VERSION_MAJOR.*~set(ARANGODB_VERSION_MAJOR      \"$VERSION_MAJOR\")~" \
  | sed -e "s~set(ARANGODB_VERSION_MINOR.*~set(ARANGODB_VERSION_MINOR      \"$VERSION_MINOR\")~" \
  | sed -e "s~set(ARANGODB_VERSION_REVISION.*~set(ARANGODB_VERSION_REVISION   \"$VERSION_REVISION\")~" \
  > CMakeLists.txt.tmp

mv CMakeLists.txt.tmp CMakeLists.txt

CMAKE_CONFIGURE="-DUSE_MAINTAINER_MODE=ON"

if [ `uname` == "Darwin" ];  then
  CMAKE_CONFIGURE="${CMAKE_CONFIGURE} -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl -DCMAKE_OSX_DEPLOYMENT_TARGET=10.11"
fi

echo "COMPILING"
rm -rf build && mkdir build

(
  cd build
  cmake .. ${CMAKE_CONFIGURE}
  make -j 8
)

echo "LINTING"
./utils/jslint.sh

git add -f \
  README \
  arangod/Aql/tokens.cpp \
  arangod/Aql/grammar.cpp \
  arangod/Aql/grammar.h \
  lib/JsonParser/json-parser.cpp \
  lib/V8/v8-json.cpp \
  lib/Basics/voc-errors.h \
  lib/Basics/voc-errors.cpp \
  js/common/bootstrap/errors.js \
  CMakeLists.txt

echo "SWAGGER"
./utils/generateSwagger.sh

echo "EXAMPLES"
./utils/generateExamples.sh

echo "GRUNT"
(
  cd js/apps/system/_admin/aardvark/APP
  rm -rf node_modules
  npm install --only=dev
  grunt deploy
)

git add -f Documentation/Examples/*.generated

if [ "$BOOK" == "1" ];  then
  echo "DOCUMENTATION"
  (cd Documentation/Books; make)
fi

case "$TAG" in
  *-alpha*|*-beta*|devel)
    ;;

  *)
    if test -f EXPERIMENTAL; then git rm -f EXPERIMENTAL; fi
    ;;
esac

if [ "$TAG" == "1" ];  then
  echo "COMMIT"

  git commit -m "release version $VERSION" -a
  git push

  git tag "v$VERSION"
  git push --tags

  echo
  echo "--------------------------------------------------"
  echo "Remember to update the VERSION in 'devel' as well."
  echo "--------------------------------------------------"
fi
