#!/bin/bash

set -e

ALLBOOKS="HTTP AQL Manual Cookbook Drivers"

GITAUTH="$1"

for book in ${ALLBOOKS}; do 

    repos=$(grep '^#' ../Books/${book}/SUMMARY.md |grep git |sed 's;#  *;;')

    for oneRepo in ${repos}; do

        export REPO=$(echo     $oneRepo |cut -d ';' -f 1)
        export CLONEDIR=$(echo $oneRepo |cut -d ';' -f 2)
        export SUBDIR=$(echo   $oneRepo |cut -d ';' -f 3)
        export SRC=$(echo      $oneRepo |cut -d ';' -f 4)
        export DST=$(echo      $oneRepo |cut -d ';' -f 5)


        export CODIR="../Books/repos/${CLONEDIR}"
        export AUTHREPO=$(echo "${REPO}" | sed "s;@;${GITAUTH}@;")
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
        branch=$(cat ../../VERSIONS|grep "EXTERNAL_DOC_${CLONEDIR}=" | sed "s/^EXTERNAL_DOC_${CLONEDIR}=//")

        if [ -z "${branch}" ]; then
            echo "no branch for ${CLONEDIR}, specify in VERSIONS file."
            exit 1
        fi

        # checkout name from VERSIONS file and pull=merge origin
        (cd "${CODIR}" && git checkout "${branch}" && git pull)

        for oneMD in $(cd "${CODIR}/${SUBDIR}"; find "./${SRC}" -type f |sed "s;\./;;"); do
            export oneMD
            export NAME=$(basename ${oneMD})
            export MDSUBDIR=$(echo "${oneMD}" | sed "s;${NAME};;")
            export DSTDIR="../Books/${book}/${DST}/${MDSUBDIR}"
            export TOPREF=$(echo ${MDSUBDIR} | sed 's;\([a-zA-Z]*\)/;../;g')
            if test ! -d "${DSTDIR}"; then
                mkdir -p "${DSTDIR}"
            fi
            (
                echo "<!-- don't edit here, its from ${REPO} / ${SUBDIR}/${SRC} -->"
                cat "${CODIR}/${SUBDIR}/${SRC}/${oneMD}" |sed "s;https://docs.arangodb.com/latest;../${TOPREF};g"
            ) > "${DSTDIR}/${NAME}"

        done
    done
done
