#!/bin/sh

export MALLOC_CONF="process_madvise_max_batch:0,experimental_hpa_start_huge_if_thp_always:true"
