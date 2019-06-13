#!/bin/bash
echo "Running on ${HOSTNAME}"

out_dir=perf_${HOSTNAME}
mkdir -p ${out_dir}

for N in 256 1024 4096 16384 65536 262144 1048576 4194304 16777216 67108864
do
    steps=`expr 4 \* 67108864 / ${N}`
    for exe in "roessler" "roessler_simd"
    do
        rm -f ${out_dir}/${exe}_N${N}.times
        for i in {0..4}
        do
            likwid-pin -cS0:0 ./${exe} ${N} ${steps} >> ${out_dir}/${exe}_N${N}.times
        done
        for perf_ctr in "FLOPS_DP" "FLOPS_AVX" "L2" "L3" "MEM"
        do
            likwid-perfctr -CS0:0 -g ${perf_ctr} ./${exe} ${N} ${steps} > ${out_dir}/${exe}_N${N}_${perf_ctr}.perf
        done
    done
done
