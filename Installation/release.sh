#!/bin/bash
set -e

SCRIPT_DIR=$(dirname "$0")
SRC_DIR="${SCRIPT_DIR}/../"
ENTERPRISE_SRC_DIR="${SRC_DIR}enterprise"

FORCE_TAG=0
TAG=1
BOOK=1
BUILD=1
SWAGGER=1
EXAMPLES=1
LINT=1
PARALLEL=8

SED=sed
IS_MAC=0

NOTIFY='if test -x /usr/games/oneko; then /usr/games/oneko& fi'
trap "$NOTIFY" EXIT

if test "$(uname)" == "Darwin"; then
    SED=gsed
    OSNAME=darwin
    IS_MAC=1
fi

if flex --version; then
    echo "flex found."
else
    echo "flex missing from your system"
    exit 1
fi

if npm --version; then
    echo "npm found."
else
    echo "npm missing from your system"
    exit 1
fi

if gitbook --version; then
    echo "gitbook found."
else
    echo "gitbook missing from your system"
    exit 1
fi

if sha1sum --version; then
    echo "sha1sum found."
else
    echo "sha1sum missing from your system"
    exit 1
fi

if [ "$#" -lt 1 ];  then
    echo "usage: $0 <major>.<minor>.<revision>"
    exit 1
fi

while [ "$#" -gt 0 ];  do
    echo "$1"
    case "$1" in
        --parallel)
            shift
            PARALLEL=$1
            shift
            ;;

        --force-tag)
            shift
            FORCE_TAG=1
            ;;

        --no-lint)
            LINT=0
            shift
            ;;

        --no-build)
            BUILD=0
            shift
            ;;

        --recycle-build)
            BUILD=2
            shift
            ;;

        --no-swagger)
            SWAGGER=0
            shift
            ;;

        --no-examples)
            EXAMPLES=0
            shift
            ;;

        --no-commit)
            TAG=0
            shift
            ;;

        --sync-user)
            shift
            DOWNLOAD_SYNCER_USER=$1
            shift
            ;;

        --no-book)
            BOOK=0
            shift
            ;;
        *)
            if test -n "${VERSION}"; then
                echo "we already have a version ${VERSION} aborting because of $1"
                exit 1
            fi
            VERSION="$1"
            shift
            ;;
    esac
done

if [ ! -d "${ENTERPRISE_SRC_DIR}" ];  then
    echo "enterprise directory missing"
    exit 1
fi

VERSION_RE='^([0-9]+.[0-9]+.[0-9]+)(-((alpha|beta|milestone|preview|rc).)?([0-9]+|devel|nightly))?$'

if echo "${VERSION}" | egrep -q -- $VERSION_RE; then
    echo "${VERSION} matches $VERSION_RE"
else
    echo "${VERSION} does not match $VERSION_RE"
    exit 1
fi

N1=`echo $VERSION | awk -F- '{print $1}'`
N2=`echo $VERSION | awk -F- '{print $2}'`

VERSION_MAJOR=$(echo "$N1" | awk -F. '{print $1}')
VERSION_MINOR=$(echo "$N1" | awk -F. '{print $2}')
VERSION_PATCH=$(echo "$N1" | awk -F. '{print $3}')

VERSION_RELEASE_TYPE=""
VERSION_RELEASE_NUMBER=""

if test ! -z "$N2"; then
  VERSION_RELEASE_TYPE=$(echo "$N2" | awk -F. '{print $1}')
  VERSION_RELEASE_NUMBER=$(echo "$N2" | awk -F. '{print $2}')
fi

echo "VERSION_MAJOR:          $VERSION_MAJOR"
echo "VERSION_MINOR:          $VERSION_MINOR"
echo "VERSION_PATCH:          $VERSION_PATCH"
echo "VERSION_RELEASE_TYPE:   $VERSION_RELEASE_TYPE"
echo "VERSION_RELEASE_NUMBER: $VERSION_RELEASE_NUMBER"

if test "${FORCE_TAG}" == 0; then
    if git tag | grep -q "^v$VERSION$";  then
        echo "$0: version $VERSION already defined"
        exit 1
    fi
fi

if fgrep -q "v$VERSION" CHANGELOG;  then
    echo "version $VERSION defined in CHANGELOG"
else
    case "$VERSION" in
        *-devel|*-nightly)
          ;;

        *)
          echo "$0: version $VERSION not defined in CHANGELOG"
          exit 1
          ;;
    esac
fi

if test -z "${DOWNLOAD_SYNCER_USER}"; then
    echo "$0: have to specify --sync-user <githublogin> to fetch the arangosync revision"
    exit 1
fi

count=0
SEQNO="$$"
while test -z "${OAUTH_REPLY}"; do
    OAUTH_REPLY=$(
        curl -s "https://$DOWNLOAD_SYNCER_USER@api.github.com/authorizations" \
             --data "{\"scopes\":[\"repo\", \"repo_deployment\"],\"note\":\"Release-tag-${SEQNO}-${OSNAME}\"}"
               )
    if test -n "$(echo "${OAUTH_REPLY}" |grep already_exists)"; then
        # retry with another number...
        OAUTH_REPLY=""
        SEQNO=$((SEQNO + 1))
        count=$((count + 1))
        if test "${count}" -gt 20; then
            echo "failed to login to github! Giving up."
            exit 1
        fi
    fi
done
OAUTH_TOKEN=$(echo "$OAUTH_REPLY" | \
                     grep '"token"'  |\
                     $SED -e 's;.*": *";;' -e 's;".*;;'
           )
OAUTH_ID=$(echo "$OAUTH_REPLY" | \
                  grep '"id"'  |\
                  $SED -e 's;.*": *;;' -e 's;,;;'
        )

# shellcheck disable=SC2064
trap "$NOTIFY; curl -s -X DELETE \"https://$DOWNLOAD_SYNCER_USER@api.github.com/authorizations/${OAUTH_ID}\"" EXIT


GET_SYNCER_REV=$(curl -s "https://api.github.com/repos/arangodb/arangosync/releases?access_token=${OAUTH_TOKEN}")

SYNCER_REV=$(echo "${GET_SYNCER_REV}"| \
                    grep tag_name | \
                    head -n 1 | \
                    ${SED} -e "s;.*: ;;" -e 's;";;g' -e 's;,;;'
          )
if test -z "${SYNCER_REV}"; then
    echo "failed to determine the revision of arangosync with github!"
    exit 1
fi

${SED} -i VERSIONS -e "s;SYNCER_REV.*;SYNCER_REV \"${SYNCER_REV}\";"

GITSHA=$(git log -n1 --pretty='%h')
if git describe --exact-match --tags "${GITSHA}"; then
    GITARGS=$(git describe --exact-match --tags "${GITSHA}")
    echo "I'm on tag: ${GITARGS}"
else
    GITARGS=$(git branch --no-color| grep '^\*' | $SED "s;\* *;;")
    if echo "$GITARGS" |grep -q ' '; then
        GITARGS=devel
    fi
    echo "I'm on Branch: ${GITARGS}"
fi

(cd enterprise; git checkout master; git fetch --tags; git pull --all; git checkout ${GITARGS}; git pull )
git fetch --tags

# recalculate SHA1 sum for JS files
rm -f js/JS_FILES.txt js/JS_SHA1SUM.txt
find js enterprise/js -type f | sort | xargs sha1sum > js/JS_FILES.txt
sha1sum js/JS_FILES.txt > js/JS_SHA1SUM.txt
rm -f js/JS_FILES.txt

# shellcheck disable=SC2002
cat CMakeLists.txt \
    | $SED -e "s~set(ARANGODB_VERSION_MAJOR.*~set(ARANGODB_VERSION_MAJOR \"$VERSION_MAJOR\")~" \
    | $SED -e "s~set(ARANGODB_VERSION_MINOR.*~set(ARANGODB_VERSION_MINOR \"$VERSION_MINOR\")~" \
    | $SED -e "s~set(ARANGODB_VERSION_PATCH.*~set(ARANGODB_VERSION_PATCH \"$VERSION_PATCH\")~" \
    | $SED -e "s~set(ARANGODB_VERSION_RELEASE_TYPE.*~set(ARANGODB_VERSION_RELEASE_TYPE \"$VERSION_RELEASE_TYPE\")~" \
    | $SED -e "s~set(ARANGODB_VERSION_RELEASE_NUMBER.*~set(ARANGODB_VERSION_RELEASE_NUMBER \"$VERSION_RELEASE_NUMBER\")~" \
          > CMakeLists.txt.tmp

mv CMakeLists.txt.tmp CMakeLists.txt

CMAKE_CONFIGURE="-DUSE_MAINTAINER_MODE=ON"

if [ "${IS_MAC}" == 1 ];  then
    CMAKE_CONFIGURE="${CMAKE_CONFIGURE} -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl -DCMAKE_OSX_DEPLOYMENT_TARGET=10.11"
fi

if [ "$BUILD" != "0" ];  then
    echo "COMPILING COMMUNITY"
    rm -rf build && mkdir build
    (
        cd build
        # shellcheck disable=SC2086
        cmake .. ${CMAKE_CONFIGURE}
        make clean_autogenerated_files
        # shellcheck disable=SC2086
        cmake .. ${CMAKE_CONFIGURE}
        make -j "${PARALLEL}"
    )

    echo "COMPILING ENTERPRISE"
    rm -rf build-enterprise && mkdir build-enterprise
    (
        cd build-enterprise
        # shellcheck disable=SC2086
        cmake .. ${CMAKE_CONFIGURE} -DUSE_ENTERPRISE=ON
        make -j "${PARALLEL}"
    )
fi

if [ "$LINT" == "1" ]; then
    echo "LINTING"
    ./utils/jslint.sh
fi

# we utilize https://developer.github.com/v3/repos/ to get the newest release of the arangodb starter:
STARTER_REV=$(curl -s https://api.github.com/repos/arangodb-helper/arangodb/releases | \
                         grep tag_name | \
                         head -n 1 | \
                         ${SED} -e "s;.*: ;;" -e 's;";;g' -e 's;,;;')
${SED} -i VERSIONS -e "s;STARTER_REV.*;STARTER_REV \"${STARTER_REV}\";"

./utils/generateREADME.sh

git add -f \
    README \
    arangod/Aql/tokens.cpp \
    arangod/Aql/grammar.cpp \
    arangod/Aql/grammar.h \
    lib/V8/v8-json.cpp \
    lib/Basics/voc-errors.h \
    lib/Basics/voc-errors.cpp \
    js/common/bootstrap/errors.js \
    js/JS_SHA1SUM.txt \
    CMakeLists.txt \
    VERSIONS

if [ "$EXAMPLES" == "1" ];  then
    echo "EXAMPLES"
    ./utils/generateExamples.sh
fi

if [ "$SWAGGER" == "1" ];  then
    echo "SWAGGER"
    ./utils/generateSwagger.sh
fi

echo "REACT"
(
    cd js/apps/system/_admin/aardvark/APP/react
    rm -rf node_modules
    npm install
    npm run build
)

git add -f Documentation/Examples/*.generated

echo "MAN"
(
    cd build
    make man
)
git add -f Documentation/man



if [ "$BOOK" == "1" ];  then
    echo "DOCUMENTATION"
    (cd Documentation/Books; make)
fi

case "$VERSION" in
    *-milestone*|*-alpha*|*-beta*|devel)
    ;;

    *)
        if test -f EXPERIMENTAL; then git rm -f EXPERIMENTAL; fi
        ;;
esac

if [ "$TAG" == "1" ];  then
    echo "COMMIT"

    git commit -m "release version $VERSION" -a
    git push

    if test "${FORCE_TAG}" == 0; then
        git tag "v$VERSION"
        git push origin "refs/tags/v$VERSION"
    else
        git tag -f "v$VERSION"
        git push -f origin "refs/tags/v$VERSION"
    fi

    cd "${ENTERPRISE_SRC_DIR}"
    git commit --allow-empty -m "release version $VERSION enterprise" -a
    git push

    if test "${FORCE_TAG}" == 0; then
        git tag "v$VERSION"
        git push origin "refs/tags/v$VERSION"
    else
        git tag -f "v$VERSION"
        git push -f origin "refs/tags/v$VERSION"
    fi

    echo
    echo "--------------------------------------------------"
    echo "Remember to update the VERSION in 'devel' as well."
    echo "--------------------------------------------------"
fi
