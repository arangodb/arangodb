#!/bin/bash

set -e

ALLBOOKS="HTTP AQL Manual Cookbook Drivers"

GITAUTH="$1"

for book in ${ALLBOOKS}; do 

    repos=$(grep '^ *#' "../Books/${book}/SUMMARY.md" |grep git |sed 's;^ *# *;;')

    for oneRepo in ${repos}; do

        REPO=$(echo     "$oneRepo" |cut -d ';' -f 1)
        CLONEDIR=$(echo "$oneRepo" |cut -d ';' -f 2)
        SUBDIR=$(echo   "$oneRepo" |cut -d ';' -f 3)
        SRC=$(echo      "$oneRepo" |cut -d ';' -f 4)
        DST=$(echo      "$oneRepo" |cut -d ';' -f 5)


        CODIR="../Books/repos/${CLONEDIR}"
        AUTHREPO="${REPO/@/${GITAUTH}@}"
        if test -d "${CODIR}"; then
            (
                cd "${CODIR}"
                git pull --all
            )
        else
            if test "${GITAUTH}" == "git"; then
                AUTHREPO=$(echo "${AUTHREPO}" | sed -e "s;github.com/;github.com:;" -e "s;https://;;" )
            fi
            git clone "${AUTHREPO}" "${CODIR}"
        fi

        # extract branch/tag/... for checkout from VERSIONS file
        branch=$(grep "EXTERNAL_DOC_${CLONEDIR}=" "../../VERSIONS" | sed "s/^EXTERNAL_DOC_${CLONEDIR}=//")

        if [ -z "${branch}" ]; then
            echo "no branch for ${CLONEDIR}, specify in VERSIONS file."
            exit 1
        fi

        # checkout name from VERSIONS file and pull=merge origin
        (cd "${CODIR}" && git checkout "${branch}" && git pull)

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
                    echo "<!-- don't edit here, its from ${REPO} / ${SUBDIR}/${SRC} -->"
                    sed "s;https://docs.arangodb.com/latest;../${TOPREF};g" "$sourcefile"
                ) > "$targetfile"
            else
                cp "$sourcefile" "$targetfile"
            fi
        done
    done
done
