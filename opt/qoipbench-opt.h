/* Option parsing code generated from JSON template */
#ifndef OPT_H
#define OPT_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct opt{
	char *custom;
	char *directory;
	int effort;
	int threads;
	int entropy;
	int iterations;
	int verbosity;
	int warmup;
	int png;
	int verify;
	int encode;
	int decode;
	int recurse;
	int _mode;
} opt_t;

/*Help function*/
int optmode_help();
/*Initialise struct*/
int opt_init(opt_t *opt);
/*Process args*/
int opt_process(opt_t *opt, int argc, char *argv[]);
/*Execute the chosen mode, if any*/
int opt_dispatch(opt_t *opt);
/*All-in-one function that could substitute for main in simple cases*/
int opt_aio(int argc, char *argv[]);
/*Mode execution functions defined elsewhere*/
int optmode_license(opt_t *opt);

/*Define OPT_C in one compilation unit for the implementation*/
#ifdef OPT_C

int opt_init(opt_t *opt){
	opt->_mode=2;
	opt->effort=1;
	opt->threads=1;
	opt->entropy=0;
	opt->iterations=1;
	opt->verbosity=1;
	opt->warmup=1;
	opt->png=1;
	opt->verify=1;
	opt->encode=1;
	opt->decode=1;
	opt->recurse=1;
	opt->custom=NULL;
	opt->directory=NULL;
	return 0;
}

int opt_process(opt_t *opt, int argc, char *argv[]){
	int loc=1, modeset=0;
	while(loc<argc){
		if(strcmp("-custom", argv[loc])==0){
				opt->custom=argv[loc+1];
				++loc;
		}
		else if(strcmp("-directory", argv[loc])==0){
				opt->directory=argv[loc+1];
				++loc;
		}
		else if(strcmp("-effort", argv[loc])==0){
			opt->effort=atoi(argv[loc+1]);
			if(opt->effort<-1){
				fprintf(stderr, "Error, -effort value must be at least -1\n");
				return 1;
			}
			else if(opt->effort>6){
				fprintf(stderr, "Error, -effort value must be at most 6\n");
				return 1;
			}
			++loc;
		}
		else if(strcmp("-threads", argv[loc])==0){
			opt->threads=atoi(argv[loc+1]);
			if(opt->threads<1){
				fprintf(stderr, "Error, -threads value must be at least 1\n");
				return 1;
			}
			else if(opt->threads>64){
				fprintf(stderr, "Error, -threads value must be at most 64\n");
				return 1;
			}
			++loc;
		}
		else if(strcmp("-entropy", argv[loc])==0){
			opt->entropy=atoi(argv[loc+1]);
			if(opt->entropy<0){
				fprintf(stderr, "Error, -entropy value must be at least 0\n");
				return 1;
			}
			else if(opt->entropy>2){
				fprintf(stderr, "Error, -entropy value must be at most 2\n");
				return 1;
			}
			++loc;
		}
		else if(strcmp("-iterations", argv[loc])==0){
			opt->iterations=atoi(argv[loc+1]);
			if(opt->iterations<0){
				fprintf(stderr, "Error, -iterations value must be at least 0\n");
				return 1;
			}
			else if(opt->iterations>999){
				fprintf(stderr, "Error, -iterations value must be at most 999\n");
				return 1;
			}
			++loc;
		}
		else if(strcmp("-verbosity", argv[loc])==0){
			opt->verbosity=atoi(argv[loc+1]);
			++loc;
		}
		else if(strcmp("-warmup", argv[loc])==0)
			opt->warmup=1;
		else if(strcmp("-no-warmup", argv[loc])==0)
			opt->warmup=0;
		else if(strcmp("-png", argv[loc])==0)
			opt->png=1;
		else if(strcmp("-no-png", argv[loc])==0)
			opt->png=0;
		else if(strcmp("-verify", argv[loc])==0)
			opt->verify=1;
		else if(strcmp("-no-verify", argv[loc])==0)
			opt->verify=0;
		else if(strcmp("-encode", argv[loc])==0)
			opt->encode=1;
		else if(strcmp("-no-encode", argv[loc])==0)
			opt->encode=0;
		else if(strcmp("-decode", argv[loc])==0)
			opt->decode=1;
		else if(strcmp("-no-decode", argv[loc])==0)
			opt->decode=0;
		else if(strcmp("-recurse", argv[loc])==0)
			opt->recurse=1;
		else if(strcmp("-no-recurse", argv[loc])==0)
			opt->recurse=0;
		else if(strcmp("-license", argv[loc])==0){
			if(modeset){
				fprintf(stderr, "Error, multiple modes defined\n");
				return 1;
			}
			else{
				modeset=1;
				opt->_mode=0;
			}
		}
		else if(strcmp("-help", argv[loc])==0){
			if(modeset){
				fprintf(stderr, "Error, multiple mode args defined\n");
				return 1;
			}
			else
				opt->_mode=1;
			++loc;
		}
		else{
			fprintf(stderr, "Error processing arguments, '%s' unknown\n", argv[loc]);
			return 1;
		}
		++loc;
	}
	return 0;
}

int opt_dispatch(opt_t *opt){
	switch(opt->_mode){
		case 0:
			return optmode_license(opt);
		case 1:
			return optmode_help();
		case 2:/*No mode requested*/
			return 0;
		default:
			fprintf(stderr, "Error, option mode invalid\n");
			return 1;
	}
}

int opt_aio(int argc, char *argv[]){
	int ret;
	opt_t opt;
	if((ret=opt_init(&opt))){
		fprintf(stderr, "Error, opt init failed\n");
		return ret;
	}
	if((ret=opt_process(&opt, argc, argv)))
		return ret;
	return opt_dispatch(&opt);
}

int optmode_help(){
	printf("\nMODES:\n");
	printf(" -license\n");
	printf("    Display license\n");

	printf("\nOPTIONS:\n");
	printf(" -custom input\n");
	printf("    Define a custom set of combinations (comma-delimited)\n\n");
	printf(" -directory input\n");
	printf("    The directory to benchmark\n\n");
	printf(" -effort input\n");
	printf("    Combination preset 0-6, higher tries more combinations, default 1.\n\n");
	printf(" -threads input\n");
	printf("    Number of threads to use. Default 1\n\n");
	printf(" -entropy input\n");
	printf("    Entropy coder to use. 0=none, 1=LZ4, 2=ZSTD (default 0)\n\n");
	printf(" -iterations input\n");
	printf("    Number of iterations to try, default 1.\n\n");
	printf(" -verbosity input\n");
	printf("    Amount of statistics to print (0:Grand total, 1:And dir stats, 2:And file stats)\n\n");
	printf(" -warmup\n");
	printf(" -no-warmup\n");
	printf("    Perform a warmup run (default true)\n");
	printf(" -png\n");
	printf(" -no-png\n");
	printf("    Run png encode/decode (default true)\n");
	printf(" -verify\n");
	printf(" -no-verify\n");
	printf("    Verify QOIP roundtrip (default true)\n");
	printf(" -encode\n");
	printf(" -no-encode\n");
	printf("    Run encoders (default true)\n");
	printf(" -decode\n");
	printf(" -no-decode\n");
	printf("    Run decoders (default true)\n");
	printf(" -recurse\n");
	printf(" -no-recurse\n");
	printf("    Descend into directories (default true)\n");

	return 0;
}

#endif /*OPT_C*/

#endif /*OPT_H*/
