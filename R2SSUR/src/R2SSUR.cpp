// [[Rcpp::plugins(cpp11)]]
// [[Rcpp::plugins(openmp)]]
// [[Rcpp::depends(RcppArmadillo)]]
// [[Rcpp::depends(BH)]]
//' @title R2SSUR_internal
//' @description
//' Run a SUR Bayesian sampler -- internal function
//' @name R2SSUR_internal
//' @param dataFile path to data file
//' @param outFilePath path to where the output is to be written
//' @param nIter number of iterations
//' @param nChains number of parallel chains to run
//
// NOTE THAT THIS IS BASICALLY JUST A WRAPPER

#include "drive.h"

// [[Rcpp::export(rng=false)]]
int R2SSUR_internal(const std::string& dataFile, const std::string& blockFile, const std::string& structureGraphFile, const std::string& outFilePath,  
                    unsigned int nIter=10, unsigned int burnin=0, unsigned int nChains=1,
                    const std::string& method="SSUR", bool sparse = true , const std::string& gammaSampler="Bandit", 
                    const std::string& gammaInit = "MLE", bool usingGPrior=false)
{
    int status = drive( dataFile, blockFile, structureGraphFile, outFilePath,
            			nIter, burnin, nChains, method, sparse, gammaSampler, gammaInit, usingGPrior );
    // Exit
    return status;  
}