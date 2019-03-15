#!/bin/bash

set -e

ALLBOOKS="HTTP AQL Manual Cookbook Drivers"

GITAUTH="$1"

for book in ${ALLBOOKS}; do

    repos=$(grep '^ *<!-- SYNC: ' "../Books/${book}/SUMMARY.md" |sed -r 's;^ *<!-- SYNC: (.+) -->$;\1;')
    reposUnique=$(awk '!seen[$0]++' <(for oneRepo in ${repos}; do echo "${oneRepo}"; done))

    for oneRepo in ${reposUnique}; do

        REPO=$(echo     "$oneRepo" |cut -d ';' -f 1)
        CLONEDIR=$(echo "$oneRepo" |cut -d ';' -f 2)
        SUBDIR=$(echo   "$oneRepo" |cut -d ';' -f 3)
        SRC=$(echo      "$oneRepo" |cut -d ';' -f 4)
        DST=$(echo      "$oneRepo" |cut -d ';' -f 5)

        CODIR="../Books/repos/${CLONEDIR}"
        AUTHREPO="${REPO/@/${GITAUTH}@}"

        if test "${GITAUTH}" == "git"; then
            AUTHREPO=$(echo "${AUTHREPO}" | sed -e "s;github.com/;github.com:;" -e "s;https://;;" )
        fi

        echo
        echo "Syncing ${AUTHREPO} -> ${CLONEDIR}"

        # extract branch/tag/... for checkout from VERSIONS file
        branch=$(grep "EXTERNAL_DOC_${CLONEDIR}=" "../../VERSIONS" | sed "s/^EXTERNAL_DOC_${CLONEDIR}=//")

        if [ -z "${branch}" ]; then
            echo "Branch for ${CLONEDIR} needs to be specified in VERSIONS file!"
            exit 1
        fi

        if test -d "${CODIR}"; then
            # checkout branch as specified in VERSIONS file, update by pulling (ff or merge)
            (
                cd "${CODIR}"
                git fetch
                git checkout "${branch}"
                git merge FETCH_HEAD # --ff-only ?
            )
        else
            # clone the desired branch directly (but fetch everything)
            git clone --branch "${branch}" "${AUTHREPO}" "${CODIR}"
        fi

        for oneMD in $(cd "${CODIR}/${SUBDIR}"; find "./${SRC}" -type f |sed "s;\./;;"); do
            NAME=$(basename "${oneMD}")
            MDSUBDIR="${oneMD/${NAME}/}"
            DSTDIR="../Books/${book}/${DST}/${MDSUBDIR}"
            TOPREF=$(echo "${MDSUBDIR}" | sed 's;\([a-zA-Z]*\)/;../;g')
            if test ! -d "${DSTDIR}"; then
                mkdir -p "${DSTDIR}"
            fi
            sourcefile="${CODIR}/${SUBDIR}/${SRC}/${oneMD}"
            targetfile="${DSTDIR}/${NAME}"
            if [[ "$sourcefile" == *.md ]]; then
                (
                    echo "<!-- don't edit here, it's from ${REPO} / ${SUBDIR}/${SRC} -->"
                    sed "s;https://docs.arangodb.com/latest;../${TOPREF};g" "$sourcefile"
                ) > "$targetfile"
            else
                cp "$sourcefile" "$targetfile"
            fi
        done
    done
done
