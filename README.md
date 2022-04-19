# Automatic Tile Size Selection Modeling

Automatic creation of tile size selection models using ML methods. Inspired by Yuki et al.'s [paper][1]. Adapted for the [ROSE compiler][3].
Training and test data sets are generated from [PolyBench 3.1][4].

## Overview

To automatically model the tile size selection problem, this project contains code that:
1. Automatically extracts relevant loop features that are relevant for deciding optimal tile size. Refer to the [paper][1] for these features.
2. Performs an empirical search for an optimal tile size for each input file.
3. Uses each optimal tile size and loop feature pair to train a number of ML models to predict an optimal tile size.
4. Uses the trained models in a ROSE pass such that unseen programs can be automatically tiled

## Usage and file descriptions

- `GenerateTiledBenchmarks.C:` A ROSE pass that, for each tile candidate loop, extracts features of the loop and outputs a program with that loop tiled to a range of different tile sizes (i.e. {1, 4, 8, 16, 32, 64, 128, 256} by default). Loop features for each test case are appended as a row to a csv file `features.csv`
- `generate_all_tiled_benchmarks.sh:` A bash file that calls `GenerateTiledBenchmarks.C` on all benchmarks in the `benchmarks/polybench-3.1` directory and stores each output to a directory named `tiled_polybench/`.
- `measure_runtimes.sh:` A bash file that measures the runtime of each tiled polybench program in `tiled_polybench/`. Stores each result as a row in a csv file `tiled_polybench/runtimes.csv`
- `notebooks/tile_size_analysis.ipynb:` A jupyter notebook that reads in the `tiled_polybench/runtimes.csv` and `tiled_polybench/features.csv` files into dataframes, performs some feature processing, preps data for training, and finally trains a number of scikit-learn classifiers to predict the empirically chosen optimal tile sizes and saves these models into the `models/` directory
- `predict_tile_size.py:` A python program that takes in loop features as command line arguments and performances inference with the trained models. Outputs the prediction into a specified file.
- `AutoTile.C:` A ROSE pass that for each tile candidate loop, extracts features of the loop, calls `predict_tile_size.py` with these features, and finally uses the predicted tile sizes to automatically tile the program

## References

[1]: Tomofumi Yuki, Lakshminarayanan Renganarayanan, Sanjay Rajopadhye, Charles Anderson, Alexandre E. Eichenberger, and Kevin O'Brien. 2010. Automatic creation of tile size selection models. In Proceedings of the 8th annual IEEE/ACM international symposium on Code generation and optimization (CGO '10). Association for Computing Machinery, New York, NY, USA, 190–199. DOI:https://doi.org/10.1145/1772954.1772982

[2]: Song Liu, Yuanzhen Cui, Qing Jiang, Qian Wang, Weiguo Wu, An efficient tile size selection model based on machine learning, Journal of Parallel and Distributed Computing, Volume 121, 2018, Pages 27-41, ISSN 0743-7315, https://doi.org/10.1016/j.jpdc.2018.06.005.

[1]: https://doi.org/10.1145/1772954.1772982

[2]: https://doi.org/10.1016/j.jpdc.2018.06.005

[3]: https://github.com/rose-compiler

[4]: https://web.cse.ohio-state.edu/~pouchet.2/software/polybench/

[5]: https://github.com/rose-compiler/rose/wiki/Install-Rose-From-Source
