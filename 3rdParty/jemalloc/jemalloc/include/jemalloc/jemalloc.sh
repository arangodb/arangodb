#!/bin/sh

objroot=$1

cat <<EOF
#ifndef JEMALLOC_H_
#define JEMALLOC_H_
#pragma GCC system_header

EOF

for hdr in jemalloc_defs.h jemalloc_rename.h jemalloc_macros.h \
           jemalloc_protos.h jemalloc_typedefs.h jemalloc_mangle.h ; do
  cat "${objroot}include/jemalloc/${hdr}" \
      | grep -v 'Generated from .* by configure\.' \
      | sed -e 's/ $//g'
  echo
done

cat <<EOF
#endif /* JEMALLOC_H_ */
EOF
