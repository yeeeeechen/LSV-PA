#!/bin/bash
rm -f temp/diff.log
for file in ./benchmarks/arithmetic/*.aig ./benchmarks/random_control/*.aig
do
  filename=$(basename -- "$file")
  filename="${filename%.*}"
  ./abc -c lsv_print_msfc  $file > temp/"$filename".txt
  echo "$filename" >> temp/diff.log
  diff lsv_fall_2021/pa1/ref/"$filename".txt temp/"$filename".txt >> temp/diff.log 
done
