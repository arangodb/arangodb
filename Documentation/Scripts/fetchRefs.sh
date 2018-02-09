#!/bin/bash


ALLBOOKS="HTTP AQL Manual Cookbook"

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
            git clone "${AUTHREPO}" "${CODIR}"
        fi

        for oneMD in $(cd "${CODIR}/${SUBDIR}"; find "./${SRC}" -type f |sed "s;\./;;"); do
            export oneMD
            export NAME=$(basename ${oneMD})
            export MDSUBDIR=$(echo "${oneMD}" | sed "s;${NAME};;")
            export DSTDIR="../Books/${book}/${DST}/${MDSUBDIR}"
            
            if test ! -d "${DSTDIR}"; then
                mkdir -p "${DSTDIR}"
            fi
            (
                echo "<!-- don't edit here, its from ${REPO} / ${SUBDIR}/${SRC} -->"
                cat "${CODIR}/${SUBDIR}/${SRC}/${oneMD}"
            ) > "${DSTDIR}/${NAME}"

        done
    done
done
