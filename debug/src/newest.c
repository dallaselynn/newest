/***************************************************************************
 *   Copyright (C) 2007 by Dallas Lynn,,,   *
 *   dallas.lynn@gmail.com   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

/* need this jank because glibc doesn't like to use nftw without it */
#define _XOPEN_SOURCE 1
#define _XOPEN_SOURCE_EXTENDED 1

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <ftw.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>

/* official name of the program */
#define PROGRAM_NAME "newer"

#define AUTHORS "Dallas Lynn"
#define AUTHOR_EMAIL "dallas.lynn@gmail.com"

/* default number of newest files to show */
#define DEFAULT_DISPLAY_FILES 1

/* what the program is actually invoked as */
char *program_name;


void 
usage(int status) {
    if( status != EXIT_SUCCESS)
        fprintf(stderr, "Try `%s --help' for more information.\n",
                program_name);
    else {
        printf("Usage: %s [OPTION]... [DIRECTORY]...\n", program_name);
        printf("Print the newest file in a directory tree to standard output\n");
        printf("By default they are displayed in descending order of last modified time.\n");

        fputs(" \
 -a, --atime              sort by access time  \n \
 -m, --mtime              sort by modified time \n \
 -c, --ctime              sort by inode update time (so-called creation time) \n \
 -n, --number=N           show the N newest files \n \
 -R, --reverse            show the oldest files instead \n \
 -h, --help               display this help message and exit \n \
 -v, --version            display version information and exit \n", stdout);

	printf("\nreport bugs to <%s>\n", AUTHOR_EMAIL);
    }	

    exit(status);
}

static struct option const long_options[] =
{
  { "atime",  no_argument,       NULL, 'a'},
  { "mtime",  no_argument,       NULL, 'm'},
  { "ctime",  no_argument,       NULL, 'c'},
  { "number", required_argument, NULL, 'n'},
  { "reverse",no_argument,       NULL, 'R'},
  { "help",   no_argument,       NULL, 'h'},
  { 0, 0, 0, 0}
};


/* stack will hold the information we're interested in ... filename and modified date */
typedef struct file_info {
  time_t sort_time;
  char *file_name;
} ENTRY;


static size_t result_total = 0;

/* holds all entries */
ENTRY **results = NULL;

/* whether to count dirs or not */


/* add an element to the results array */
int push_result(time_t sort_time, const char *file_name);

/* print all of the entries desired */
void show_output();

/* for qsort */
int entry_compare(const void *f1p, const void *f2p);

/* callback function for nftw */
int process(const char *file, const struct stat *sb, int flag, struct FTW *s);


int 
main(int argc, char *argv[])
{
  int c, atime, ctime, mtime, reverse=1;
  int flags = FTW_PHYS;
  unsigned long n_to_display = DEFAULT_DISPLAY_FILES;
  struct stat stbuf; 
  char *dir_name, *strerr;

  program_name = argv[0];

  while((c = getopt_long(argc, argv, "amcn:Rh", long_options, NULL)) != -1) 
  {
    switch(c) 
    {
      case 'a':
          if (ctime)
              usage(EXIT_FAILURE);

          atime = 1;
          break;
      case 'c':
          if (atime)
              usage(EXIT_FAILURE);

          ctime = 1;
          break;
      case 'n':
          while (isspace (*optarg)) optarg++;

          if(optarg[0] == '-') {
              fprintf(stderr,"negative numbers make no sense here Johnny\n");
              usage(EXIT_FAILURE);
          } 

          n_to_display = (int) strtol(optarg,&strerr,10);

          /* if strerr is anything other than empty it means there was something
           * that could not be converted, ie. an error 
           */
          if(*strerr != '\0' || !n_to_display) {
            fprintf(stderr, "%s is not a valid number argument!\n", optarg);
            usage(EXIT_FAILURE);
          }
          break;
      case 'h':
          usage(EXIT_SUCCESS);
          break;
      case 'R':
          reverse = 1;
          break;
      case 0:
          break;
      case ':':
          fprintf(stderr, "%s: option '-%c' requires an argument\n", argv[0],
                  optopt);
          usage(EXIT_FAILURE);
      case '?':
          fprintf(stderr, "%s: option '-c%' is invalid: ignored\n", argv[0], 
                  optopt);
          usage(EXIT_FAILURE);
      default:
          usage(EXIT_FAILURE);
    }
  }

  dir_name = argv[1];
  printf("findin' newest %d in %s son\n", n_to_display, dir_name);

  if(stat(dir_name, &stbuf) < 0) {
    fprintf(stderr,"%s aint' even there son\n", dir_name);
    return(EXIT_FAILURE);	
  }

  if(! S_ISDIR(stbuf.st_mode)) {
    fprintf(stderr,"%s ain't no directory son\n", dir_name);
    return(EXIT_FAILURE);
  }

  /* FIXME: pick a non-stupid value for fd max */
  if(nftw(dir_name, process, 10000, flags) != 0) {
    fprintf(stderr,"%s stopped early", dir_name);
  }

  qsort(results, result_total ,sizeof(ENTRY *), entry_compare);
  show_output(results);

  /* FIXME: actually free the filename strings also */
  free(results);

  return EXIT_SUCCESS;
}

int
push_result(time_t sort_time, const char *file_name) 
{
  ENTRY new;
 
  /* FIXME: test result and fail gracefully */
  results = (ENTRY **)realloc(results, (result_total + 1) * sizeof(ENTRY *));

  /* FIXME: use calloc */
  if((results[result_total] = (ENTRY *) malloc(sizeof(ENTRY))) == NULL) {
    fprintf(stderr,"FATAL: out of memory");
    exit(EXIT_FAILURE);
  }

  results[result_total]->sort_time = sort_time;
  results[result_total]->file_name = (char *) malloc(strlen(file_name) + 1);
  strncpy(results[result_total]->file_name,file_name,strlen(file_name)+1);
  result_total++;
}

int 
process(const char *file, const struct stat *sb, int flag, struct FTW *s ) 
{
  int retval = 0;
  double d;

  switch(flag) {
    case FTW_DNR:
      fprintf(stderr, "unable to read %s\n", file);
      break;
    case FTW_NS:
      fprintf(stderr, "could not stat file %s: %s\n", file, strerror(errno));
      break;
    case FTW_D:
      return retval;
    case FTW_F:
      push_result(sb->st_ctime, file); 
      break;
  }

  return retval;
}

void
show_output()
{
  int i;

  if(results == NULL) return;

  for(i = 0; i < result_total; i++){
    printf("%d\n",results[i]->sort_time); 
  }
}

/* for qsort to sort the stack of files we made */
int
entry_compare(const void *e1p, const void *e2p)
{
  const ENTRY *e1 = *(ENTRY * const *)e1p;
  const ENTRY *e2 = *(ENTRY * const *)e2p;
  double diff;

  diff = difftime(e1->sort_time, e2->sort_time);

  if(diff < 0)
    return -1;
  else if(diff > 0)
    return 1;
  else 
    return 0;
}
