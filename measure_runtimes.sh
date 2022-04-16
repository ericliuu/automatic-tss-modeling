#!/bin/bash

cd tiled_polybench

# create a .csv file if it doesn't exist
if [ ! -f runtimes.csv ]; then
  touch runtimes.csv
  echo "uniqueFilename,run1,run2,run3,run4,run5" >> runtimes.csv
fi

# for each binary, output the runtimes of 5 separate runs to a .csv file
for binary in *.out; do

  # check if we already measured this binary
  if grep -Fq "$binary" runtimes.csv; then
    echo "Skipping measurement for $binary"
    continue
  fi

  echo "Starting measurements for $binary"
  RUN1=$(./$binary)
  RUN2=$(./$binary)
  RUN3=$(./$binary)
  RUN4=$(./$binary)
  RUN5=$(./$binary)
  RUN6=$(./$binary)
  RUN7=$(./$binary)
  RUN8=$(./$binary)
  echo "$binary,$RUN1,$RUN2,$RUN3,$RUN4,$RUN5" >> runtimes.csv

done 
