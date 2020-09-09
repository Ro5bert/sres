/* Copy me if you can.
 * by 20h
 * Taken from suckless.org and modified. */

extern char *argv0;

/* use main(int argc, char **argv) */
#define ARGBEGIN for (argv0 = *argv, argv++, argc--;\
                 	  argv[0] && argv[0][0] == '-' && argv[0][1];\
                 	  argv++, argc--) {\
                 	char argcurr_;\
                 	bool brk_;\
                 	int i_;\
                 	if (argv[0][1] == '-' && argv[0][2] == '\0') {\
                 		argv++;\
                 		argc--;\
                 		break;\
                 	}\
                 	for (i_ = 1, brk_ = false;\
                 	     !brk_ && argv[0][i_];\
                 	     i_++) {\
                 		argcurr_ = argv[0][i_];\
                 		switch (argcurr_)

#define ARGEND   	}\
                 }

#define ARGCURR() argcurr_

#define EARGF(x) (brk_ = true, (argv[0][i_+1] == '\0' && argv[1] == NULL) ?\
                  	((x), abort(), (char *)0) :\
                  	((argv[0][i_+1] != '\0') ?\
                  		(&argv[0][i_+1]) :\
                  		(argv++, argc--, argv[0])))

#define ARGF() (brk_ = true, (argv[0][i_+1] == '\0' && argv[1] == NULL) ?\
                	(char *)0 :\
                	((argv[0][i_+1] != '\0') ?\
                		(&argv[0][i_+1]) :\
                		(argv++, argc--, argv[0])))
