/* Option parsing code generated from JSON template */
#ifndef OPT_H
#define OPT_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct opt{
	size_t in_len;
	char *in;
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
	opt->_mode=3;
	opt->in=NULL;
	opt->in_len=0;
	return 0;
}

int opt_process(opt_t *opt, int argc, char *argv[]){
	int loc=1, modeset=0;
	while(loc<argc){
		if(strcmp("-in", argv[loc])==0){
			if(opt_fread_fully(&(opt->in), &(opt->in_len), argv[loc+1])){
				fprintf(stderr, "Offending file: '%s'\n", argv[loc+1]);
				return 1;
			}
			++loc;
		}
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
		else if(strcmp("-list", argv[loc])==0){
			if(modeset){
				fprintf(stderr, "Error, multiple modes defined\n");
				return 1;
			}
			else{
				modeset=1;
				opt->_mode=1;
			}
		}
		else if(strcmp("-help", argv[loc])==0){
			if(modeset){
				fprintf(stderr, "Error, multiple mode args defined\n");
				return 1;
			}
			else
				opt->_mode=2;
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
			return optmode_list(opt);
		case 2:
			return optmode_help();
		case 3:/*No mode requested*/
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
	printf(" -list\n");
	printf("    List all known ops. id's can be used to build custom opstring's\n");

	printf("\nIO:\n");
	printf(" -in path\n");
	printf("    QOIP file to stat\n");

	return 0;
}

#endif /*OPT_C*/

#endif /*OPT_H*/
