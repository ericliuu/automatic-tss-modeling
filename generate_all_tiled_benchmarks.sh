#!/bin/bash

if [ ! -d tiled_polybench ]; then
  mkdir -p tiled_polybench;
fi

cd tiled_polybench
 
declare -a StringArray=(
"../benchmarks/polybench-3.1/linear-algebra/kernels/symm/symm.c"
"../benchmarks/polybench-3.1/linear-algebra/kernels/cholesky/cholesky.c"
"../benchmarks/polybench-3.1/linear-algebra/kernels/gemm/gemm.c"
"../benchmarks/polybench-3.1/linear-algebra/kernels/syr2k/syr2k.c"
"../benchmarks/polybench-3.1/linear-algebra/kernels/3mm/3mm.c"
"../benchmarks/polybench-3.1/linear-algebra/kernels/atax/atax.c"
"../benchmarks/polybench-3.1/linear-algebra/kernels/syrk/syrk.c"
"../benchmarks/polybench-3.1/linear-algebra/kernels/2mm/2mm.c"
"../benchmarks/polybench-3.1/linear-algebra/kernels/gemver/gemver.c"
"../benchmarks/polybench-3.1/linear-algebra/kernels/bicg/bicg.c"
"../benchmarks/polybench-3.1/linear-algebra/kernels/trmm/trmm.c"
"../benchmarks/polybench-3.1/linear-algebra/kernels/doitgen/doitgen.c"
"../benchmarks/polybench-3.1/linear-algebra/kernels/trisolv/trisolv.c"
"../benchmarks/polybench-3.1/linear-algebra/kernels/gesummv/gesummv.c"
"../benchmarks/polybench-3.1/linear-algebra/kernels/mvt/mvt.c"
"../benchmarks/polybench-3.1/linear-algebra/solvers/durbin/durbin.c"
"../benchmarks/polybench-3.1/linear-algebra/solvers/lu/lu.c"
"../benchmarks/polybench-3.1/linear-algebra/solvers/gramschmidt/gramschmidt.c"
"../benchmarks/polybench-3.1/linear-algebra/solvers/dynprog/dynprog.c"
"../benchmarks/polybench-3.1/linear-algebra/solvers/ludcmp/ludcmp.c"
"../benchmarks/polybench-3.1/stencils/jacobi-2d-imper/jacobi-2d-imper.c"
"../benchmarks/polybench-3.1/stencils/fdtd-2d/fdtd-2d.c"
"../benchmarks/polybench-3.1/stencils/fdtd-apml/fdtd-apml.c"
"../benchmarks/polybench-3.1/stencils/adi/adi.c"
"../benchmarks/polybench-3.1/stencils/jacobi-1d-imper/jacobi-1d-imper.c"
"../benchmarks/polybench-3.1/stencils/seidel-2d/seidel-2d.c"
"../benchmarks/polybench-3.1/medley/reg_detect/reg_detect.c"
"../benchmarks/polybench-3.1/medley/floyd-warshall/floyd-warshall.c"
"../benchmarks/polybench-3.1/datamining/correlation/correlation.c"
"../benchmarks/polybench-3.1/datamining/covariance/covariance.c"
)

for path in ${StringArray[@]}; do
  echo "Starting generation for $path"
  SECONDS=0
  ../GenerateTiledBenchmarks -I../benchmarks/polybench-3.1/utilities ../benchmarks/polybench-3.1/utilities/polybench.c $path -lm -DPOLYBENCH_TIME
  echo "- finished in $SECONDS seconds"
done
