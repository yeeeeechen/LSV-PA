#!/bin/bash


pa_cmd="lsv_print_msfc"
pa_dir="lsv_fall_2021/pa1"
ref_dir="${pa_dir}/ref"
out_dir="${pa_dir}/out"
diff_dir="${pa_dir}/diff"
bench_dir="benchmarks"
# modify your benchmark path
bench_list=( $(find -L ../benchmarks-master/arithmetic/ -type f -name '*.aig') 
             $(find -L ../benchmarks-master/random_control/ -type f -name '*.aig') )
students=( $(cut -d, -f1 < lsv_fall_2021/admin/participants-id.csv | tail -n +3) )


grade_one_self(){
    for bench in "${bench_list[@]}"; do
        echo "[INFO] Testing with ${bench} ..."
        bch_name=$(echo "${bench}" | awk -F "/" '{print $(NF)}' | sed -e 's/.aig$//')
        ./abc -c "${pa_cmd}" "${bench}" > "${out_dir}/${bch_name}.txt"
        diff -uwiB "${ref_dir}/${bch_name}.txt" "${out_dir}/${bch_name}.txt" > "${diff_dir}/${bch_name}.txt"
        if [ "$?" -eq 0 ]; then
            ((++correct))
            echo "${bench},Pass" 
        else
            echo "${bench},Fail" 
        fi
    done
}

    echo "[INFO] Grading branch $1 ..."
    grade_one_self "$1" point

