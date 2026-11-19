#!/bin/sh

if [ "x${enable_prof}" = "x1" ] ; then
  export MALLOC_CONF="prof:true,prof_active:false,prof_gdump:true,lg_prof_sample:0"
fi

