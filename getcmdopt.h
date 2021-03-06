#ifndef GETOPT_H
#define GETOPT_H

#ifdef NETWARE
/*
As NetWare LibC has optind and optarg macros defined in unistd.h our local variables were getting mistakenly preprocessed so undeffing optind and optarg
*/
#undef optarg
#undef optind
#endif

/* Define structure for one recognized option (both single char and long name).
 * If short_open is '-' this is the last option. */
typedef struct _opt_struct {
	char opt_char;
	int  need_param;
	char * opt_name;
} opt_struct;

/* holds the index of the latest fetched element from the opts array */
int getcmdopt(int argc, char* const *argv, const opt_struct opts[], char **optarg, int *optind, int show_err, int arg_start);

#endif
