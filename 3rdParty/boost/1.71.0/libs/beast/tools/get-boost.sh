#! /bin/sh

set -e

build_dir=$2

branch="master"

if [ "$1" != "master" -a "$1" != "refs/heads/master" ]; then
    branch="develop"
fi

echo "BUILD_DIR: $build_dir"
echo "BRANCH: $branch"

git clone -b $branch --depth 1 https://github.com/boostorg/boost.git boost-root
cd boost-root

# Use a reasonably large depth to prevent intermittent update failures due to
# commits being on a submodule's master before the superproject is updated.
git submodule update --init --depth 20 --jobs 4 \
    libs/array \
    libs/headers \
    tools/build \
    tools/boost_install \
    tools/boostdep \
    libs/align \
    libs/asio \
    libs/assert \
    libs/config \
    libs/core \
    libs/endian \
    libs/filesystem \
    libs/intrusive \
    libs/locale \
    libs/optional \
    libs/smart_ptr \
    libs/static_assert \
    libs/system \
    libs/throw_exception \
    libs/type_traits \
    libs/utility \
    libs/winapi \
    libs/algorithm \
    libs/array \
    libs/atomic \
    libs/bind \
    libs/chrono \
    libs/concept_check \
    libs/container \
    libs/container_hash \
    libs/context \
    libs/conversion \
    libs/coroutine \
    libs/date_time \
    libs/detail \
    libs/exception \
    libs/function \
    libs/function_types \
    libs/functional \
    libs/fusion \
    libs/integer \
    libs/io \
    libs/iterator \
    libs/lambda \
    libs/lexical_cast \
    libs/logic \
    libs/math \
    libs/move \
    libs/mp11 \
    libs/mpl \
    libs/numeric/conversion \
    libs/pool \
    libs/predef \
    libs/preprocessor \
    libs/random \
    libs/range \
    libs/ratio \
    libs/rational \
    libs/thread \
    libs/tuple \
    libs/type_index \
    libs/typeof \
    libs/unordered

echo Submodule update complete

rm -rf libs/beast
cp -r $build_dir libs/beast
