/* Option parsing code generated from JSON template */
#ifndef OPT_H
#define OPT_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct opt{
	char *custom;
	char *in;
	char *out;
	int effort;
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

/*Define OPT_C in one compilation unit for the implementation*/
#ifdef OPT_C

int opt_init(opt_t *opt){
	opt->_mode=1;
	opt->effort=1;
	opt->custom=NULL;
	opt->in=NULL;
	opt->out=NULL;
	return 0;
}

int opt_process(opt_t *opt, int argc, char *argv[]){
	int loc=1, modeset=0;
	while(loc<argc){
		if(strcmp("-custom", argv[loc])==0){
				opt->custom=argv[loc+1];
				++loc;
		}
		else if(strcmp("-in", argv[loc])==0){
				opt->in=argv[loc+1];
				++loc;
		}
		else if(strcmp("-out", argv[loc])==0){
				opt->out=argv[loc+1];
				++loc;
		}
		else if(strcmp("-effort", argv[loc])==0){
			opt->effort=atoi(argv[loc+1]);
			if(opt->effort<0){
				fprintf(stderr, "Error, -effort value must be at least 0\n");
				return 1;
			}
			else if(opt->effort>5){
				fprintf(stderr, "Error, -effort value must be at most 5\n");
				return 1;
			}
			++loc;
		}
		else if(strcmp("-help", argv[loc])==0){
			if(modeset){
				fprintf(stderr, "Error, multiple mode args defined\n");
				return 1;
			}
			else
				opt->_mode=0;
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
			return optmode_help();
		case 1:/*No mode requested*/
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
	printf("\nOPTIONS:\n");
	printf(" -custom input\n");
	printf("    Define a custom set of combinations (comma-delimited)\n\n");
	printf(" -in input\n");
	printf("    Input path\n\n");
	printf(" -out input\n");
	printf("    Output path\n\n");
	printf(" -effort input\n");
	printf("    Combination preset 0-5, higher tries more combinations, default 1.\n\n");

	return 0;
}

#endif /*OPT_C*/

#endif /*OPT_H*/
