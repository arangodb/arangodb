#!/bin/sh

if [ "x${enable_prof}" = "x1" ] ; then
  export MALLOC_CONF="prof:true,prof_active:true,lg_prof_sample:0,prof_recent_alloc_max:3,prof_sys_thread_name:true"
fi
