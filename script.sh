#!/bin/bash
# how to use
#./script.sh r10943XXX mac


if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <ALL|branch-name> <mac|linux>"
    echo "ALL: grade ALL students"
    echo "branch-name: the branch to grade, e.g., d04943019"
    exit 1
fi

time_limit="60s"
pa_cmd="lsv_or_bidec"
checker_cmd="lsv_pa2_checker"
pa_dir="lsv_fall_2021/pa2"
ref_dir="${pa_dir}/ref"
out_dir="${pa_dir}/out"
refprog="./abc.$2"
bench_dir="../benchmarks-master"
bench_list=("${bench_dir}/random_control/arbiter.aig" 
            "${bench_dir}/random_control/cavlc.aig"
            "${bench_dir}/random_control/ctrl.aig"
            "${bench_dir}/random_control/dec.aig"
            "${bench_dir}/random_control/i2c.aig"
            "${bench_dir}/random_control/int2float.aig"
            "${bench_dir}/random_control/priority.aig"
            "${bench_dir}/random_control/router.aig"
            "${bench_dir}/arithmetic/bar.aig")
students=( $(cut -d, -f1 < lsv_fall_2021/admin/participants-id.csv | tail -n +3) )


grade_one_branch () {
    
    student="$1"
    result="${pa_dir}/${student}.csv"
    
    rm src/ext-lsv/*.o src/ext-lsv/*.d \
        src/sat/bsat/*.o src/sat/bsat/*.d \
        src/sat/cnf/*.o src/sat/cnf/*.d \
        src/proof/fra/*.o src/proof/fra/*.d \
        src/base/abci/*.o src/base/abci/*.d
    make -j8
    echo "Benchmark,Result" > "${result}"
    rm -rf "${out_dir}" "${ref_dir}"
    mkdir -p "${out_dir}"
    mkdir -p "${ref_dir}"
    declare -i correct=0
    for bench in "${bench_list[@]}"; do
        echo "[INFO] Testing with ${bench} ..."
        bch_name=$(echo "${bench}" | awk -F "/" '{print $(NF)}' | sed -e 's/.aig$//')
         ./abc -c "${pa_cmd}" "${bench}" > "${out_dir}/${bch_name}.txt"
        ${refprog} -c "${checker_cmd} ${out_dir}/${bch_name}.txt" "${bench}" > "${ref_dir}/${bch_name}.txt" 
        match=$(grep -o -c "Pass!" ${ref_dir}/${bch_name}.txt )  
        if [ ${match} -eq 1 ]; then
                ((++correct))
            echo "${bench},Pass" >> "${result}"
        else
            echo "${bench},Fail" >> "${result}"
        fi 
    done
    local __return_var="$2"
    local __point=$(echo "${correct}*11+1" | bc)
    eval "${__return_var}"="${__point}"
    echo "[INFO] Correct cases: ${correct}"
    echo "[INFO] Total points: ${__point}"
    echo "Score,${__point}" >> "${result}"
}

if [ "$1" = "ALL" ]; then
    echo "[INFO] can't Grade all students ..."
    
else
    echo "[INFO] Grading your $1 ..."
    grade_one_branch "$1" point 
fi
