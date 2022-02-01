#!/usr/bin/env bash

set -e

PROGNAME="$(basename "${0}")"
function usage() {
cat <<EOF
Usage:
${PROGNAME} <next-version>

This script merges 'develop' to 'master' and tags a release on the 'master'
branch. It then bumps the version on 'develop' to the next planned version.
<next-version> must be of the form X.Y.Z, where X, Y and Z are numbers.

This script should be run from the root of the repository without any
uncommitted changes.
EOF
}

for arg in $@; do
    if [[ "${arg}" == "-h" || "${arg}" == "--help" ]]; then
        usage
        exit 0
    fi
done

if [[ $# -ne 1 ]]; then
    echo "Missing the version"
    usage
    exit 1
fi

next_version="${1}"
next_major="$(echo ${next_version} | cut -d '.' -f 1)"
next_minor="$(echo ${next_version} | cut -d '.' -f 2)"
next_patch="$(echo ${next_version} | cut -d '.' -f 3)"
if [[ -z "${next_major}" || -z "${next_minor}" || -z "${next_patch}" ]]; then
    echo "The version was specified incorrectly"
    usage
    exit 1
fi

current_major="$(sed -n -E 's/#define BOOST_HANA_MAJOR_VERSION ([0-9]+)/\1/p' include/boost/hana/version.hpp)"
current_minor="$(sed -n -E 's/#define BOOST_HANA_MINOR_VERSION ([0-9]+)/\1/p' include/boost/hana/version.hpp)"
current_patch="$(sed -n -E 's/#define BOOST_HANA_PATCH_VERSION ([0-9]+)/\1/p' include/boost/hana/version.hpp)"
current_version="${current_major}.${current_minor}.${current_patch}"

git checkout --quiet master
git merge --quiet develop -m "Merging 'develop' into 'master' for ${current_version}"
echo "Merged 'develop' into 'master' -- you should push 'master' when ready"

tag="v${current_major}.${current_minor}.${current_patch}"
git tag -a --file=RELEASE_NOTES.md "${tag}"
echo "Created tag ${tag} on 'master' -- you should push it when ready"

git checkout --quiet develop
sed -i '' -E "s/#define BOOST_HANA_MAJOR_VERSION [0-9]+/#define BOOST_HANA_MAJOR_VERSION ${next_major}/" include/boost/hana/version.hpp
sed -i '' -E "s/#define BOOST_HANA_MINOR_VERSION [0-9]+/#define BOOST_HANA_MINOR_VERSION ${next_minor}/" include/boost/hana/version.hpp
sed -i '' -E "s/#define BOOST_HANA_PATCH_VERSION [0-9]+/#define BOOST_HANA_PATCH_VERSION ${next_patch}/" include/boost/hana/version.hpp
cat <<EOF > RELEASE_NOTES.md
Release notes for Hana ${next_version}
============================
EOF
git add include/boost/hana/version.hpp RELEASE_NOTES.md
git commit --quiet -m "Bump version of Hana to ${next_version} and clear release notes"
echo "Bumped the version of Hana on 'develop' to ${next_version} -- you should push 'develop' when ready"
