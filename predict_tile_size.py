import os
import sys
import argparse

import pandas as pd
import numpy as np

from sklearn.model_selection import train_test_split
from sklearn.ensemble import RandomForestClassifier
from sklearn.linear_model import LinearRegression
from sklearn.preprocessing import StandardScaler
from sklearn import metrics
from sklearn.metrics import mean_squared_error, r2_score
from sklearn import svm
from sklearn.neural_network import MLPClassifier

import pickle

def load_model_from_file(filename):
    with open(filename, 'rb') as file:
        return pickle.load(file)

def main():
  parser = argparse.ArgumentParser(
      description='Predicts tile size when given the required loop features.')
  parser.add_argument(
      "readInvariant",
      help="Number of read invariant array references in the dominating loop",
      type=int)
  parser.add_argument(
      "readPrefetched",
      help="Number of read prefetched array references in the dominating loop",
      type=int)
  parser.add_argument(
      "readNonPrefetched",
      help="Number of read non-prefetched array references in the dominating loop",
      type=int)
  parser.add_argument(
      "writeInvariant",
      help="Number of write invariant array references in the dominating loop",
      type=int)
  parser.add_argument(
      "writePrefetched",
      help="Number of write prefetched array references in the dominating loop",
      type=int)
  parser.add_argument(
      "writeNonPrefetched",
      help="Number of write non-prefetched array references in the dominating loop",
      type=int)
  parser.add_argument(
      "distToDominatingLoop",
      help="Distance of the tiled loop frmo the dominating loop",
      type=int)
  parser.add_argument(
      "modelPath",
      help="Path to the trained model",
      type=str)
  parser.add_argument(
      "outputPath",
      help="Path that the predicted tile size will be written to",
      type=str)

  args = parser.parse_args()

  input = pd.DataFrame({'readInvariant': [args.readInvariant],
                        'readPrefetched': [args.readPrefetched],
                        'readNonPrefetched': [args.readNonPrefetched],
                        'writeInvariant': [args.writeInvariant],
                        'writePrefetched': [args.writePrefetched],
                        'writeNonPrefetched': [args.writeNonPrefetched],
                        'distToDominatingLoop': [args.distToDominatingLoop]})

  model = load_model_from_file(args.modelPath)

  prediction = model.predict(input)
  f = open(args.outputPath, "w")
  f.write(str(prediction[0]))
  f.close()
  
if __name__=="__main__":
    main()
