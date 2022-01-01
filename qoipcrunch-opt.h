/* Option parsing code generated from JSON template */
#ifndef OPT_H
#define OPT_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct opt{
	size_t in_len;
	FILE *out;
	char *in;
	char *effort;
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
int optmode_list(opt_t *opt);

/*Define OPT_C in one compilation unit for the implementation*/
#ifdef OPT_C

static inline int opt_fread_fully(char **dest, size_t *dest_len, char *path){
	FILE *io;
	long tell;
	if((io=fopen(path, "rb"))==NULL){
		perror("Error, 'data' fopen failed");
		return 1;
	}
	if(fseek(io, 0, SEEK_END)==-1){
		perror("Error, 'data' fseek failed");
		return 1;
	}
	if((tell=ftell(io))==-1){
		perror("Error, 'data' ftell failed");
		return 1;
	}
	*dest_len=tell;
	rewind(io);
	*dest=malloc(*dest_len);
	if(*dest_len>0 && *dest==NULL){
		perror("Error, 'data' malloc failed");
		return 1;
	}
	if(fread(*dest, 1, *dest_len, io)!=*dest_len){
		perror("Error, 'data' fread failed");
		return 1;
	}
	fclose(io);
	return 0;
}

int opt_init(opt_t *opt){
	opt->_mode=2;
	opt->effort=NULL;
	opt->in=NULL;
	opt->in_len=0;
	opt->out=NULL;
	return 0;
}

int opt_process(opt_t *opt, int argc, char *argv[]){
	int loc=1, modeset=0;
	while(loc<argc){
		if(strcmp("-out", argv[loc])==0){
			if((opt->out=fopen(argv[loc+1], "wb"))==NULL){
				perror("Error, 'stream' fopen failed");
				fprintf(stderr, "Offending file: '%s'\n", argv[loc+1]);
				return 1;
			}
			++loc;
		}
		else if(strcmp("-in", argv[loc])==0){
			if(opt_fread_fully(&(opt->in), &(opt->in_len), argv[loc+1])){
				fprintf(stderr, "Offending file: '%s'\n", argv[loc+1]);
				return 1;
			}
			++loc;
		}
		else if(strcmp("-effort", argv[loc])==0){
				opt->effort=argv[loc+1];
				++loc;
		}
		else if(strcmp("-list", argv[loc])==0){
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
			return optmode_list(opt);
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
	printf(" -list\n");
	printf("    List all known ops\n");

	printf("\nIO:\n");
	printf(" -out path\n");
	printf("    Output path\n");
	printf(" -in path\n");
	printf("    Input path\n");

	printf("\nOPTIONS:\n");
	printf(" -effort input\n");
	printf("    Define a custom set of combinations (comma-delimited), or use an effort level (0..3, where 0 tries some common combinations, 1..3 try increasingly-exhaustive brute-force combinations\n\n");

	return 0;
}

#endif /*OPT_C*/

#endif /*OPT_H*/
