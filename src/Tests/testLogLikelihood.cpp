/* Conditional Random Fields for Object Localization
 * Master thesis source code
 * 
 * Authors:
 * Andreas Christian Eilschou (jwb226@alumni.ku.dk)
 * Andreas Hjortgaard Danielsen (gxn961@alumni.ku.dk)
 *
 * Department of Computer Science
 * University of Copenhagen
 * Denmark
 *
 * Date: 27-08-2012
 */

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>

#include "Types.h"
#include "DataManager.h"
#include "ObjectiveFunctions/LogLikelihood.h"
#include "ObjectiveFunctions/LogLikelihoodGradient.h"
#include "Learning/LBFGS.h"
#include "Measures/LossMeasures.h"


using namespace std;


int main(int argc, char **argv) {

short stepSize = 16; // stepSize = 1 to use ESS. Otherwise use sliding window
int computeFiniteDifferenceGradient = 1;
int computeWeights = 0;
int computeLossOfWeights = 0;

  // initialize data manager
  DataManager dataman;
    
  try {
    //dataman.loadImages("../pascal/USURF3K/", "subsets/train_width_height.txt");
    //dataman.loadBboxes("../pascal/Annotations/ess/bicycle_train.ess");
    dataman.loadImages("../cows-train/EUCSURF-3000/", "../subsets/cows_train_width_height.txt");
    dataman.loadBboxes("../cows-train/Annotations/TUcow_train.ess");
  }
  catch (int e) {
    if (e == FILE_NOT_FOUND) {
      fprintf(stderr, "There was a problem opening a file!\n");
      return -1;
    }
  }
 
  ConditionalRandomField crf(&dataman);
  crf.setStepSize(stepSize);
  
  // create log-likelihood objective function with regularization
  LogLikelihood loglik(&dataman, &crf);
  loglik.setLambda(1000.0);//(2.0); no regularization
  
  LogLikelihoodGradient loglikgrad(&dataman, &crf);
  loglikgrad.setLambda(1000.0);//(2.0); no regularization

  cout << "StepSize: " << loglik.getStepSize() << " " << loglikgrad.getStepSize() << endl;

  // set random weights
  int num_weights = 3000;
  Weights w(num_weights);
  
  // initialize weights between -1 and 1
  srand(time(NULL)); rand();
  for (int i=0; i<num_weights; i++) {
    w[i] = 0.0 + i*0.00001; //((double) rand() / RAND_MAX)*2 - 1;
  }
  
  crf.setWeights(w);
  printf("%d\n", num_weights);
  
  
  // test evaluate
  printf("Computing log-likelihood...\n");
  double fval   = loglik.evaluate(w);
  printf("Loglikelihood = %.6f\n", fval);

  printf("Computing gradient...\n");
  Dvector grad(num_weights);
  loglikgrad.evaluate(grad, w);
  printf("Gradient = (%.2f, %.2f, %.2f, ...)\n", grad[0], grad[1], grad[2]);
  
  // print gradient
  //for (size_t i=0; i<num_weights; i++) {
  //  printf("grad[%d] = %.6f\n", (int) i, grad[i]);
  //}

  
  if(computeFiniteDifferenceGradient) {
  
  printf("Computing finite difference approximation...\n");
  double h, fx;
  Dvector wh(w); 
  Dvector fdgrad(num_weights);
  Dvector fxh(num_weights);
  h = 1e-8;

  //num_weights = 10;

  // new w
  for (int i=0; i<num_weights; i++) {
    printf("%i\n", i);
    wh[i]   += h;
    fxh[i]  = loglik.evaluate(wh);
    wh[i]   -= h;
  }
  
  fx  = loglik.evaluate(w);
  
  // compute and print gradient
  double tol = 0.1;
  for (int i=0; i<num_weights; i++) {
    fdgrad[i] = (fxh[i] - fx)/h;
    if ((grad[i] - fdgrad[i] > tol) || (fdgrad[i] - grad[i] > tol)) {
      printf("Gradient approximation differs with more than %.6f:\n\t grad[%d] = %.6f\t fdgrad[%d] = %.6f\n", tol, i, grad[i], i, fdgrad[i]);     
    }    
  }
  
  printf("Done!\n");
  
  }
  
  if (computeWeights) {
  
  double startTimeComputeWeights = gettime();
  
  // train parameters with BFGS
  LBFGS lbfgs(&loglik, &loglikgrad);
  Weights w_new(num_weights, 0.0);
  
  printf("Learning parameters with LBFGS...\n");
  try {
    w_new = lbfgs.learnWeights(w);
  } 
  catch (int e) {
    if (e == ROUNDOFF_ERROR) {
      fprintf(stderr, "BFGS::learnWeights threw a round-off error!\n");
      return ROUNDOFF_ERROR;
    }
    
    if (e == DIM_ERROR) {
      fprintf(stderr, "BFGS::learnWeights threw a dimensionality error!\n");
      return DIM_ERROR;
    }

    if (e == NOT_A_NUMBER) {
      fprintf(stderr, "BFGS::learnWeights threw a NAN error!\n");
      return NOT_A_NUMBER;
    }
  }
  
  double endTimeComputeWeights = gettime();
  printf("Computing weights took %.6f seconds.\n", endTimeComputeWeights - startTimeComputeWeights);  
  
  // print result
  //for (size_t i=0; i<10; i++) {
  //  printf("w[%d] = %.6f\n", (int) i, w_new[i]);
  //}
  
  ofstream weight_file("log_likelihood_weights_cow.txt");
  if (!weight_file) {
    cerr << "Could not open file weights.txt" << endl;
  }
  
  // store result in file
  for (int i=0; i<num_weights; i++) {
    //printf("w[%d] = %.6f\n", (int) i, w_new[i]);
    weight_file << w_new[i] << endl;
  }

  weight_file.close();
  
  cout << "Done!" << endl;
  
  }
  
  if (computeLossOfWeights) {
    
    string pathImages = "../cows-train/EUCSURF-3000/";
    string pathSubset = "../subsets/cows_train_width_height.txt";
    string pathBboxes = "../cows-train/Annotations/TUcow_train.ess";
    string pathWeights = "log_likelihood_weights_cow.txt";
    
    DataManager lossDataMan(pathImages, pathBboxes, pathSubset, pathWeights);
    
    string plotName = "test_output";
    printRecallOverlap(plotName, lossDataMan, stepSize, true);


  }
  
  return 0;
}
