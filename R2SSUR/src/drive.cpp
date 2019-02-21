#include "drive.h"

#ifndef CCODE
#include <Rcpp.h>
#endif

using std::cout;
using std::endl;

#ifdef _OPENMP
extern omp_lock_t RNGlock; //defined in global.h
#endif
extern std::vector<std::mt19937_64> rng;

int drive_SUR( Chain_Data& chainData )
{

	// ****************************************
	// **********  INIT THE CHAIN *************
	// ****************************************
	cout << "Initialising the (SUR) MCMC Chain ... " << std::flush;

	ESS_Sampler<SUR_Chain> sampler( chainData.surData , chainData.nChains , 1.2 ,
        	chainData.gamma_sampler_type, chainData.gamma_type, chainData.beta_type, chainData.covariance_type);

	// *****************************

	sampler[0] -> gammaInit( chainData.gammaInit );
	sampler[0] -> betaInit( chainData.betaInit );
    sampler[0] -> updateQuantities();
    sampler[0] -> logLikelihood();
	sampler[0] -> stepSigmaRhoAndBeta();

	// *****************************

	// set when the JT move should start
	unsigned int jtStartIteration = chainData.nIter/10;
	for( unsigned int i=0; i< chainData.nChains; ++i)
		sampler[i]->setJTStartIteration( jtStartIteration );

	// ****************************************

	cout << " DONE!\nDrafting the output files with the start of the chain ... " << std::flush;


	// INIT THE FILE OUTPUT
	std::string outFilePrefix = chainData.outFilePath+chainData.filePrefix;

	// clear the content of previous files
	std::ofstream logPOutFile; logPOutFile.open(outFilePrefix+"logP_out.txt", std::ios::out | std::ios::trunc); logPOutFile.close();
	// openlogP file in append mode
	logPOutFile.open( outFilePrefix+"logP_out.txt" , std::ios_base::app); // note we don't close!
	// open avg files in trunc mode to cut previous content
	std::ofstream gammaOutFile; gammaOutFile.open( outFilePrefix+"gamma_out.txt" , std::ios_base::trunc); gammaOutFile.close();
	std::ofstream gOutFile; gOutFile.open( outFilePrefix+"G_out.txt" , std::ios_base::trunc); gOutFile.close();
	std::ofstream piOutFile; piOutFile.open( outFilePrefix+"pi_out.txt" , std::ios_base::trunc); piOutFile.close();
	std::ofstream htpOutFile; htpOutFile.open( outFilePrefix+"hotspot_tail_p_out.txt" , std::ios_base::trunc); htpOutFile.close();

	// Output to file the initial state (if burnin=0)
	arma::umat gamma_out; // out var for the gammas
	
	arma::umat g_out; // out var for G
	arma::mat beta_out; // out var for the betas
	arma::mat sigmaRho_out; // out var for the sigmas and rhos
	arma::vec tmpVec; // temporary to store the pi parameter vector
	arma::vec pi_out;
	arma::vec hotspot_tail_prob_out;

	if( chainData.burnin == 0 )
	{
		gamma_out = sampler[0] -> getGamma(); 
		g_out = arma::umat( sampler[0] -> getGAdjMat() );
		beta_out = sampler[0] -> getBeta();
		sigmaRho_out  = sampler[0] -> getSigmaRho();
		tmpVec = sampler[0] -> getPi();
		pi_out = tmpVec;
		tmpVec.for_each( [](arma::vec::elem_type& val) { if(val>1.0) val = 1.0; else val=0.0; } );
		hotspot_tail_prob_out = tmpVec;

		gammaOutFile.open( outFilePrefix+"gamma_out.txt" , std::ios_base::trunc);
		gammaOutFile << (arma::conv_to<arma::mat>::from(gamma_out)) << std::flush;
		gammaOutFile.close();

		gOutFile.open( outFilePrefix+"G_out.txt" , std::ios_base::trunc);
		gOutFile << ( arma::conv_to<arma::mat>::from(g_out) ) << std::flush;   // this might be quite long...
		gOutFile.close();

		logPOutFile << 	sampler[0] -> getLogPTau() << " ";
		logPOutFile << 	sampler[0] -> getLogPEta() <<  " ";
		logPOutFile << 	sampler[0] -> getLogPJT() <<  " ";
		logPOutFile << 	sampler[0] -> getLogPSigmaRho() <<  " ";
		logPOutFile << 	sampler[0] -> getLogPO() <<  " ";
		logPOutFile << 	sampler[0] -> getLogPPi() <<  " ";
		logPOutFile << 	sampler[0] -> getLogPGamma() <<  " ";
		logPOutFile << 	sampler[0] -> getLogPW() <<  " ";
		logPOutFile << 	sampler[0] -> getLogPBeta() <<  " ";
		logPOutFile << 	sampler[0] -> getLogLikelihood();
		logPOutFile << 	endl << std::flush;

		piOutFile.open( outFilePrefix+"pi_out.txt" , std::ios_base::trunc);
		piOutFile << pi_out << std::flush;
		piOutFile.close();

		htpOutFile.open( outFilePrefix+"hotspot_tail_p_out.txt" , std::ios_base::trunc);
		htpOutFile << hotspot_tail_prob_out << std::flush;
		htpOutFile.close();
	}
					

	// ########
	// ########
	// ######## Start
	// ########
	// ########

	cout << "DONE! \n\nStarting "<< chainData.nChains <<" (parallel) chain(s) for " << chainData.nIter << " iterations:" << endl << std::flush;

	unsigned int tick = 1000; // how many iter for each print?

	for(unsigned int i=1; i < chainData.nIter ; ++i)
	{

		sampler.step();
		
		// #################### END LOCAL MOVES

		// ## Global moves
		// *** end Global move's section

		// UPDATE OUTPUT STATE
		if( i >= chainData.burnin )
		{
			gamma_out += sampler[0] -> getGamma(); // the result of the whole procedure is now my new mcmc point, so add that up
			g_out += arma::umat( sampler[0] -> getGAdjMat() );

			beta_out += sampler[0] -> getBeta();
			sigmaRho_out += sampler[0] -> getSigmaRho();	

			tmpVec = sampler[0] -> getPi();
			pi_out += tmpVec;
			tmpVec.for_each( [](arma::vec::elem_type& val) { if(val>1.0) val = 1.0; else val=0.0; } );
			hotspot_tail_prob_out += tmpVec;
		}

		// Print something on how the chain is going
		if( (i+1) % tick == 0 )
		{

			cout << " Running iteration " << i+1 << " ... local Acc Rate: ~ gamma: " << Utils::round( sampler[0] -> getGammaAccRate() , 3 );
			cout << " -- JT: " << Utils::round( sampler[0] -> getJTAccRate() , 3 ) ;

			if( chainData.nChains > 1)
				cout << " -- Global: " << Utils::round( sampler.getGlobalAccRate() , 3 ) << endl; 
			else
				cout << endl;

			// Output to files every now and then
			if( (i >= chainData.burnin) && ( (i-chainData.burnin+1) % (tick*1) == 0 ) )
			{

				gammaOutFile.open( outFilePrefix+"gamma_out.txt" , std::ios_base::trunc);
				gammaOutFile << (arma::conv_to<arma::mat>::from(gamma_out))/(double)(i+1.0-chainData.burnin) << std::flush;
				gammaOutFile.close();

				gOutFile.open( outFilePrefix+"G_out.txt" , std::ios_base::trunc);
				gOutFile << ( arma::conv_to<arma::mat>::from(g_out) )/((double)(i-jtStartIteration-chainData.burnin)+1.0) << std::flush;   // this might be quite long...
				gOutFile.close();

				logPOutFile << 	sampler[0] -> getLogPTau() << " ";
				logPOutFile << 	sampler[0] -> getLogPEta() <<  " ";
				logPOutFile << 	sampler[0] -> getLogPJT() <<  " ";
				logPOutFile << 	sampler[0] -> getLogPSigmaRho() <<  " ";
				logPOutFile << 	sampler[0] -> getLogPO() <<  " ";
				logPOutFile << 	sampler[0] -> getLogPPi() <<  " ";
				logPOutFile << 	sampler[0] -> getLogPGamma() <<  " ";
				logPOutFile << 	sampler[0] -> getLogPW() <<  " ";
				logPOutFile << 	sampler[0] -> getLogPBeta() <<  " ";
				logPOutFile << 	sampler[0] -> getLogLikelihood();
				logPOutFile << 	endl << std::flush;

				piOutFile.open( outFilePrefix+"pi_out.txt" , std::ios_base::trunc);
				piOutFile << pi_out/(double)(i+1.0-chainData.burnin) << std::flush;
				piOutFile.close();

				htpOutFile.open( outFilePrefix+"hotspot_tail_p_out.txt" , std::ios_base::trunc);
				htpOutFile << hotspot_tail_prob_out/(double)(i+1.0-chainData.burnin) << std::flush;
				htpOutFile.close();

				#ifndef CCODE
				Rcpp::checkUserInterrupt(); // this checks for interrupts from R ... or does it?
				#endif

			}

		}

	} // end MCMC


	// Print the end
	cout << " MCMC ends. " /* << " Final temperature ratio ~ " << temperatureRatio  */<< "  --- Saving results and exiting" << endl;

	// ### Collect results and save them
	gammaOutFile.open( outFilePrefix+"gamma_out.txt" , std::ios_base::trunc);
	gammaOutFile << (arma::conv_to<arma::mat>::from(gamma_out))/(double)(chainData.nIter-chainData.burnin+1.) << std::flush;
	gammaOutFile.close();

	gOutFile.open( outFilePrefix+"G_out.txt" , std::ios_base::trunc);
	gOutFile << ( arma::conv_to<arma::mat>::from(g_out) )/(double)(chainData.nIter-jtStartIteration-chainData.burnin+1.) << std::flush;   // this might be quite long...
	gOutFile.close();

	logPOutFile << 	sampler[0] -> getLogPTau() << " ";
	logPOutFile << 	sampler[0] -> getLogPEta() <<  " ";
	logPOutFile << 	sampler[0] -> getLogPJT() <<  " ";
	logPOutFile << 	sampler[0] -> getLogPSigmaRho() <<  " ";
	logPOutFile << 	sampler[0] -> getLogPO() <<  " ";
	logPOutFile << 	sampler[0] -> getLogPPi() <<  " ";
	logPOutFile << 	sampler[0] -> getLogPGamma() <<  " ";
	logPOutFile << 	sampler[0] -> getLogPW() <<  " ";
	logPOutFile << 	sampler[0] -> getLogPBeta() <<  " ";
	logPOutFile << 	sampler[0] -> getLogLikelihood();
	logPOutFile << 	endl << std::flush;
	logPOutFile.close();

	// ----
	beta_out = beta_out/(double)(chainData.nIter-chainData.burnin+1);
	beta_out.save(outFilePrefix+"beta_out.txt",arma::raw_ascii);

	sigmaRho_out = sigmaRho_out/(double)(chainData.nIter-chainData.burnin+1);
	sigmaRho_out.save(outFilePrefix+"sigmaRho_out.txt",arma::raw_ascii);
	// -----

	// -----
	piOutFile.open( outFilePrefix+"pi_out.txt" , std::ios_base::trunc);
	piOutFile << pi_out/(double)(chainData.nIter-chainData.burnin+1) << std::flush;
	piOutFile.close();

	htpOutFile.open( outFilePrefix+"hotspot_tail_p_out.txt" , std::ios_base::trunc);
	htpOutFile << hotspot_tail_prob_out/(double)(chainData.nIter-chainData.burnin+1) << std::flush;
	htpOutFile.close();
	// -----


	cout << "Saved to :   "+outFilePrefix+"****_out.txt" << endl;
	cout << "Final w : " << sampler[0] -> getW() <<  endl;
	cout << "Final tau : " << sampler[0] -> getTau() << "    w/ proposal variance: " << sampler[0] -> getVarTauProposal() << endl;
	cout << "Final eta : " << sampler[0] -> getEta() <<  endl;
	// cout << "Final o : " << sampler[0] -> getO().t() << "       w/ proposal variance: " << sampler[0] -> getVarOProposal() << endl;  
	// cout << "Final pi : " << sampler[0] -> getPi().t() << "       w/ proposal variance: " << sampler[0] -> getVarPiProposal() << endl;
	cout << "  -- Average Omega : " << arma::accu( sampler[0] -> getO() * sampler[0] -> getPi().t() )/((double)(sampler[0]->getP()*sampler[0]->getS())) <<  endl;
	if( chainData.nChains > 1 ) 
		cout << "Final temperature ratio : " << sampler[1]->getTemperature() <<  endl << endl ;

	// Exit

	cout << "DONE, exiting! " << endl << endl ;
	return 0;
}

int drive_HESS( Chain_Data& chainData )
{

	// ****************************************
	// **********  INIT THE CHAIN *************
	// ****************************************
	cout << "Initialising the (HESS) MCMC Chain ... " << std::flush;

	ESS_Sampler<HESS_Chain> sampler( chainData.surData , chainData.nChains , 1.2 ,
        	chainData.gamma_sampler_type, chainData.gamma_type, chainData.beta_type, chainData.covariance_type);

	// *****************************
	
	sampler[0] -> gammaInit( chainData.gammaInit ); // this updates gammaMask as well
	sampler[0] -> logLikelihood();

	// ****************************************

	cout << " DONE!\nDrafting the output files with the start of the chain ... " << std::flush;

	// INIT THE FILE OUTPUT
	std::string outFilePrefix = chainData.outFilePath+chainData.filePrefix;

	// clear the content of previous files
	std::ofstream logPOutFile; logPOutFile.open(outFilePrefix+"logP_out.txt", std::ios::out | std::ios::trunc); logPOutFile.close();
	// openlogP file in append mode
	logPOutFile.open( outFilePrefix+"logP_out.txt" , std::ios_base::app); // note we don't close!
	// open avg files in trunc mode to cut previous content
	std::ofstream gammaOutFile; gammaOutFile.open( outFilePrefix+"gamma_out.txt" , std::ios_base::trunc); gammaOutFile.close();
	std::ofstream piOutFile; piOutFile.open( outFilePrefix+"pi_out.txt" , std::ios_base::trunc); piOutFile.close();
	std::ofstream htpOutFile; htpOutFile.open( outFilePrefix+"hotspot_tail_p_out.txt" , std::ios_base::trunc); htpOutFile.close();

	// Output to file the initial state (if burnin=0)
	arma::umat gamma_out; // out var for the gammas
	
	arma::vec tmpVec; // temporary to store the pi parameter vector
	arma::vec pi_out;
	arma::vec hotspot_tail_prob_out;

	if( chainData.burnin == 0 )
	{
		gamma_out = sampler[0] -> getGamma(); 
		tmpVec = sampler[0] -> getPi();
		pi_out = tmpVec;
		tmpVec.for_each( [](arma::vec::elem_type& val) { if(val>1.0) val = 1.0; else val=0.0; } );
		hotspot_tail_prob_out = tmpVec;

		gammaOutFile.open( outFilePrefix+"gamma_out.txt" , std::ios_base::trunc);
		gammaOutFile << (arma::conv_to<arma::mat>::from(gamma_out)) << std::flush;
		gammaOutFile.close();

		logPOutFile << 	sampler[0] -> getLogPO() <<  " ";
		logPOutFile << 	sampler[0] -> getLogPPi() <<  " ";
		logPOutFile << 	sampler[0] -> getLogPGamma() <<  " ";
		logPOutFile << 	sampler[0] -> getLogPW() <<  " ";
		logPOutFile << 	sampler[0] -> getLogLikelihood();
		logPOutFile << 	endl << std::flush;

		piOutFile.open( outFilePrefix+"pi_out.txt" , std::ios_base::trunc);
		piOutFile << pi_out << std::flush;
		piOutFile.close();

		htpOutFile.open( outFilePrefix+"hotspot_tail_p_out.txt" , std::ios_base::trunc);
		htpOutFile << hotspot_tail_prob_out << std::flush;
		htpOutFile.close();
	}


	// ########
	// ########
	// ######## Start
	// ########
	// ########

	cout << "Starting "<< chainData.nChains <<" (parallel) chain(s) for " << chainData.nIter << " iterations:" << endl << std::flush;

	unsigned int tick = 1000; // how many iter for each print?

	for(unsigned int i=1; i < chainData.nIter ; ++i)
	{

		sampler.step();
		
		// #################### END LOCAL MOVES

		// ## Global moves
		// *** end Global move's section

		// UPDATE OUTPUT STATE
		if( i >= chainData.burnin )
		{
			gamma_out += sampler[0] -> getGamma(); // the result of the whole procedure is now my new mcmc point, so add that up

			tmpVec = sampler[0] -> getPi();
			pi_out += tmpVec;
			tmpVec.for_each( [](arma::vec::elem_type& val) { if(val>1.0) val = 1.0; else val=0.0; } );
			hotspot_tail_prob_out += tmpVec;
		}

		// Print something on how the chain is going
		if( (i+1) % tick == 0 )
		{

			cout << " Running iteration " << i+1 << " ... local Acc Rate: ~ gamma: " << Utils::round( sampler[0] -> getGammaAccRate() , 3 );

			if( chainData.nChains > 1)
				cout << " -- Global: " << Utils::round( sampler.getGlobalAccRate() , 3 ) << endl; 
			else
				cout << endl;
				
			// Output to files every now and then
			if( (i >= chainData.burnin) && ( (i-chainData.burnin+1) % (tick*1) == 0 ) ) 
			{

				gammaOutFile.open( outFilePrefix+"gamma_out.txt" , std::ios_base::trunc);
				gammaOutFile << (arma::conv_to<arma::mat>::from(gamma_out))/(double)(i+1.0-chainData.burnin) << std::flush;
				gammaOutFile.close();

				logPOutFile << 	sampler[0] -> getLogPO() <<  " ";
				logPOutFile << 	sampler[0] -> getLogPPi() <<  " ";
				logPOutFile << 	sampler[0] -> getLogPGamma() <<  " ";
				logPOutFile << 	sampler[0] -> getLogPW() <<  " ";
				logPOutFile << 	sampler[0] -> getLogLikelihood();
				logPOutFile << 	endl << std::flush;

				piOutFile.open( outFilePrefix+"pi_out.txt" , std::ios_base::trunc);
				piOutFile << pi_out/(double)(i+1.0-chainData.burnin) << std::flush;
				piOutFile.close();

				htpOutFile.open( outFilePrefix+"hotspot_tail_p_out.txt" , std::ios_base::trunc);
				htpOutFile << hotspot_tail_prob_out/(double)(i+1.0-chainData.burnin) << std::flush;
				htpOutFile.close();

				#ifndef CCODE
				Rcpp::checkUserInterrupt(); // this checks for interrupts from R ... or does it?
				#endif

			}

		}

	} // end MCMC


	// Print the end
	cout << " MCMC ends. " /* << " Final temperature ratio ~ " << temperatureRatio  */<< "  --- Saving results and exiting" << endl;

	// ### Collect results and save them
	gammaOutFile.open( outFilePrefix+"gamma_out.txt" , std::ios_base::trunc);
	gammaOutFile << (arma::conv_to<arma::mat>::from(gamma_out))/(double)(chainData.nIter-chainData.burnin+1) << std::flush;
	gammaOutFile.close();

	logPOutFile << 	sampler[0] -> getLogPO() <<  " ";
	logPOutFile << 	sampler[0] -> getLogPPi() <<  " ";
	logPOutFile << 	sampler[0] -> getLogPGamma() <<  " ";
	logPOutFile << 	sampler[0] -> getLogPW() <<  " ";
	logPOutFile << 	sampler[0] -> getLogLikelihood();
	logPOutFile << 	endl << std::flush;
	logPOutFile.close();

	// -----
	piOutFile.open( outFilePrefix+"pi_out.txt" , std::ios_base::trunc);
	piOutFile << pi_out/(double)(chainData.nIter-chainData.burnin+1) << std::flush;
	piOutFile.close();

	htpOutFile.open( outFilePrefix+"hotspot_tail_p_out.txt" , std::ios_base::trunc);
	htpOutFile << hotspot_tail_prob_out/(double)(chainData.nIter-chainData.burnin+1) << std::flush;
	htpOutFile.close();
	// -----

	cout << "Saved to :   "+outFilePrefix+"****_out.txt" << endl;
	cout << "Final w : " << sampler[0] -> getW() << "       w/ proposal variance: " << sampler[0] -> getVarWProposal() << endl;  
	// cout << "Final o : " << sampler[0] -> getO().t() << "       w/ proposal variance: " << sampler[0] -> getVarOProposal() << endl;  
	// cout << "Final pi : " << sampler[0] -> getPi().t() << "       w/ proposal variance: " << sampler[0] -> getVarPiProposal() << endl;
	cout << "  -- Average Omega : " << arma::accu( sampler[0] -> getO() * sampler[0] -> getPi().t() )/((double)(sampler[0]->getP()*sampler[0]->getS())) <<  endl;
	if( chainData.nChains > 1 ) 
		cout << "Final temperature ratio : " << sampler[1]->getTemperature() <<  endl << endl ;

	// Exit

	cout << "DONE, exiting! " << endl << endl ;
	return 0;

}


// *******************************************************************************
// *******************************************************************************
// *******************************************************************************


int drive( const std::string& dataFile, const std::string& blockFile, const std::string& structureGraphFile, const std::string& outFilePath,  
			unsigned int nIter, unsigned int burnin, unsigned int nChains,
			const std::string& method, const bool sparse, const std::string& gammaSampler, const std::string& gammaInit, bool usingGPrior )
{

	cout << "R2SSUR -- Bayesian Sparse Seemingly Unrelated Regression Modelling" << endl;
	
	#ifdef _OPENMP
	std::cout << "Using OpenMP" << std::endl;
	omp_init_lock(&RNGlock);  // init RNG lock for the parallel part
	#endif

	// ###########################################################
	// ###########################################################
	// ## Read Arguments and Data
	// ###########################################################
	// ###########################################################

	// Declare all the data-related variables
	Chain_Data chainData; // this initialises the pointers and the strings to ""

	chainData.nChains = nChains;
	chainData.nIter = nIter;
	chainData.burnin = burnin;

	chainData.outFilePath = outFilePath;

	if ( gammaSampler == "Bandit" )
		chainData.gamma_sampler_type = Gamma_Sampler_Type::bandit ;
	else if ( gammaSampler == "MC3" )
		chainData.gamma_sampler_type = Gamma_Sampler_Type::mc3 ;
	else
		throw Bad_Gamma_Sampler_Type{};

	if ( usingGPrior )
		chainData.beta_type = Beta_Type::gprior ;
	else 
		chainData.beta_type = Beta_Type::independent ;

	// *****************************************************
	if( chainData.beta_type == Beta_Type::gprior )
	{
		std::cerr << "ERROR: GPrior not implemented yet";
		return 1;
	}
	// *****************************************************

	if ( method == "SUR" && sparse == true )
		chainData.covariance_type = Covariance_Type::sparse;
	else
		chainData.covariance_type = Covariance_Type::dense;

	// read Data and format into usables
	cout << "Reading input ... ";

	Utils::formatData(dataFile, blockFile, structureGraphFile, chainData.surData );

	cout << " successfull!" << endl;

	// ############

	// The intercept column to X will be inserted when initialising the chain

	cout << "Clearing and initialising output files " << endl;
	// Re-define dataFile so that I can use it in the output
	chainData.filePrefix = dataFile;
	std::size_t slash = chainData.filePrefix.find("/");  // remove the path from filePrefix
	while( slash != std::string::npos )
	{
		chainData.filePrefix.erase(chainData.filePrefix.begin(),chainData.filePrefix.begin()+slash+1);
		slash = chainData.filePrefix.find("/");
	}
	chainData.filePrefix.erase(chainData.filePrefix.begin()+chainData.filePrefix.find(".txt"),chainData.filePrefix.end());  // remove the .txt from filePrefix !

	// Update the "outFilePath" (filePrefix variable) with the method's name
	chainData.filePrefix += "_";
	if( method == "SUR" )
	{
		if( sparse )
			chainData.filePrefix += "S";
		else
			chainData.filePrefix += "d";
	}
	chainData.filePrefix += method+"_";

	cout << "Init RNG engine .. ";

	// ############# Init the RNG generator/engine
	std::random_device r;
	unsigned int nThreads{1};
	
	#ifdef _OPENMP
		nThreads = omp_get_max_threads();
		omp_set_num_threads( std::min( (unsigned int)16, nThreads ) ); //make 16 as parameter TODO
	#endif

	rng.reserve(nThreads);  // reserve the correct space for the vector of rng engines
	std::seed_seq seedSeq;	// and declare the seedSequence
	std::vector<unsigned int> seedInit(8);
	long long int seed = std::chrono::system_clock::now().time_since_epoch().count();

	// seed all the engines
	for(unsigned int i=0; i<nThreads; ++i)
	{
		rng[i] = std::mt19937_64(seed + i*(1000*(std::pow(chainData.surData.nOutcomes,3)*chainData.surData.nPredictors*3)*nIter) );
	}

	cout << " DONE ! " << endl;

	// ###################################
	// Parameters Inits
	// ###################################


	// cout<< chainData.surData.data->n_rows << " " << chainData.surData.data->n_cols << endl;
	// cout<< chainData.surData.nObservations << " " << chainData.surData.nOutcomes<< " " << chainData.surData.nFixedPredictors<< " " << chainData.surData.nVSPredictors << endl;
	// cout<< (*chainData.surData.outcomesIdx).t() << (*chainData.surData.fixedPredictorsIdx).t() << (*chainData.surData.VSPredictorsIdx).t() << endl;


	if ( gammaInit == "R" )
	{
	// Random Init
		chainData.gammaInit = arma::umat(chainData.surData.nVSPredictors,chainData.surData.nOutcomes); // init empty
		for(unsigned int j=0; j<chainData.surData.nVSPredictors; ++j)
			for(unsigned int l=0; l< chainData.surData.nOutcomes; ++l)
				chainData.gammaInit(j,l) = Distributions::randBernoulli( 0.5 );

	}else if( gammaInit == "1" ){
		// Static Init ***
		// ** 1
		chainData.gammaInit = arma::ones<arma::umat>(chainData.surData.nVSPredictors,chainData.surData.nOutcomes);

	}else if ( gammaInit == "0" ) {
		// ** 0
		chainData.gammaInit = arma::zeros<arma::umat>(chainData.surData.nVSPredictors,chainData.surData.nOutcomes);

	}else if ( gammaInit == "MLE" ) {
		// ** MLE
		arma::mat Q,R; arma::qr(Q,R, chainData.surData.data->cols( arma::join_vert( *chainData.surData.fixedPredictorsIdx , *chainData.surData.VSPredictorsIdx ) ) );
		
		chainData.betaInit = arma::solve(R,arma::trans(Q) * chainData.surData.data->cols( *chainData.surData.outcomesIdx ) );
		chainData.gammaInit = chainData.betaInit > 0.5*arma::stddev(arma::vectorise(chainData.betaInit));

		if( chainData.surData.nFixedPredictors > 0 )
			chainData.gammaInit.shed_rows( 0 , chainData.surData.nFixedPredictors-1 ); // shed the fixed preditors rows since we don't have gammas for those

	}else{
		// default case
		chainData.gammaInit = arma::zeros<arma::umat>(chainData.surData.nVSPredictors,chainData.surData.nOutcomes);
	}

	// ###################################
	// Samplers
	// ###################################

	int status;

	// TODO, I hate this, but I can't initialise/instanciate templated classes
	// at runtime so this seems fair (given that the different drive functions have their differences in output and stuff...)
	// still if there's a more elegant solution I'd like to find it

	if( method == "SUR" )
		status = drive_SUR(chainData);
	else if( method == "HESS" )
		status = drive_HESS(chainData);
	else
		status = drive_SUR(chainData); // this makes a default
			// you should't reach here even if method is wrongly specified, but still...

	return status;
}