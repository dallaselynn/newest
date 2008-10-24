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

#include <stdbool.h>
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
#include <locale.h>

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
 -d, --include-dirs       include directories in list instead of only plain files \n \
 -q, --quiet              suppress timestamp info.  show only filenames \n \
 -H, --human              print timestamp in more friendly format than epoch time \n \
 -e, --ignore-empty       don't include empty files in the results \n \
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
  { "include-dirs", no_argument, NULL, 'd'},
  { "quiet",  no_argument,       NULL, 'q'},
  { "human",  no_argument,       NULL, 'H'},
  { "ignore-empty", no_argument, NULL, 'e' },
  { NULL, 0, NULL, 0}
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
static int include_dirs = false;

/* default is to use mtime, so start these false */
static bool use_atime = false;
static bool use_ctime = false;

/* sort oldest to newest instead if set */
static bool reverse = false;

/* display times not in epoch but in something less sortable
   and more readable */
static bool human_readable = false;

/* results to return */
static unsigned long n_to_display = DEFAULT_DISPLAY_FILES;

/* supress timestamp if true  */
static bool quiet_output = false;

/* skip empty filesi in output if true */
static bool ignore_empty = false;

/* add an element to the results array */
int push_result(const struct stat *hb, const char *file_name);

/* print all of the entries desired */
void show_output();

/* for qsort */
int entry_compare(const void *f1p, const void *f2p);

/* callback function for nftw */
int process(const char *file, const struct stat *sb, int flag, struct FTW *s);


int 
main(int argc, char *argv[])
{
  int c, flags = FTW_PHYS;
  struct stat stbuf; 
  char *dir_name, *strerr;

  program_name = argv[0];

  setlocale(LC_ALL,"");

  while((c = getopt_long(argc, argv, "amcn:RhdqtHe", long_options, NULL)) != -1)
  {
    switch(c) 
    {
      case 'a':
          if (use_ctime) {
              fprintf(stderr, "can't yet sort by both ctime and atime.  need to pick just one\n");
              usage(EXIT_FAILURE);
          }

          use_atime = true;
          break;
      case 'c':
          if (use_atime) {
              fprintf(stderr, "can't yet sort by both ctime and atime.  need to pick just one\n");
              usage(EXIT_FAILURE);
          }

          use_ctime = true;
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
          reverse = true;
          break;
      case 'd':
          include_dirs = true;
          break;
      case 'q':
          quiet_output = true;
          break;
      case 'H':
          human_readable = true;
          break;
      case 'e':
          ignore_empty = true;
          break;
      case 0:
          break;
      case ':':
          fprintf(stderr, "%s: option '-%c' requires an argument\n", argv[0],
                  optopt);
          usage(EXIT_FAILURE);
      case '?':
          fprintf(stderr, "%s: option '-%c' is invalid: ignored\n", argv[0], 
                  optopt);
          usage(EXIT_FAILURE);
      default:
          usage(EXIT_FAILURE);
    }
  }

  if(argc - optind == 0){
    fprintf(stderr, "you did nay give no dir to search\n");
    return(EXIT_FAILURE);
  }

  while(optind < argc) {
    dir_name = argv[optind++];

    printf("findin' newest %d in %s son\n", n_to_display, dir_name);
	
    if(stat(dir_name, &stbuf) < 0) {
      fprintf(stderr,"%s aint' even there son\n", dir_name);
      return(EXIT_FAILURE);	
    }
	
	if(! S_ISDIR(stbuf.st_mode)) {
	  fprintf(stderr,"%s ain't no directory son\n", dir_name);
	  return(EXIT_FAILURE);
	}
	
	/* FIXME: pick a non-arbitrary value for fd max */
	if(nftw(dir_name, process, 10000, flags) != 0) {
      fprintf(stderr,"%s stopped early", dir_name);
	}
  }

  qsort(results, result_total ,sizeof(ENTRY *), entry_compare);

  show_output();

  /* C ain't all wine and roses */
  while(result_total--) {
      free(results[result_total]->file_name);
      free(results[result_total]);
  }

  free(results);

  return EXIT_SUCCESS;
}

int
push_result(const struct stat *sb, const char *file_name) 
{
  ENTRY new;
  const char *fn;
  fn = file_name;
 
  results = (ENTRY **)realloc(results, (result_total + 1) * sizeof(ENTRY *));
  if(!results) {
    fprintf(stderr, "FATAL: out of memory");
    exit(EXIT_FAILURE);
  }

  if((results[result_total] = (ENTRY *) malloc(sizeof(ENTRY))) == NULL) {
    fprintf(stderr,"FATAL: out of memory");
    exit(EXIT_FAILURE);
  }

  if(use_atime)
    results[result_total]->sort_time = sb->st_atime;
  else if(use_ctime)
    results[result_total]->sort_time = sb->st_ctime;
  else 
    results[result_total]->sort_time = sb->st_mtime;

  /* changed mind...keep leading ./ 
   * if(*(file_name)=='.' && *(file_name + 1)=='/')
   *   fn = file_name + 2;
   */

  results[result_total]->file_name = strdup(file_name);
  result_total++;
}

int 
process(const char *file, const struct stat *sb, int flag, struct FTW *s ) 
{
  int retval = 0;
  double d;

  if( ignore_empty && !sb->st_size )
    return retval;

  switch(flag) {
    case FTW_DNR:
      fprintf(stderr, "unable to read %s\n", file);
      break;
    case FTW_NS:
      fprintf(stderr, "could not stat file %s: %s\n", file, strerror(errno));
      break;
    case FTW_D:
      if(include_dirs) 
        push_result(sb, file);
      break;
    case FTW_F:
      push_result(sb, file); 
      break;
  }

  return retval;
}

void
show_output()
{
  int i;
  struct tm *t;
  char buf[100];

  if(results == NULL) return;

  printf("showing %d results\n",n_to_display);

  for(i = 0; i < n_to_display; i++){
      if (quiet_output) {
        printf("%s\n",results[i]->file_name); 
      } else {
        if(human_readable) {
          /* FIXME: be smarter about buf size */
          t = localtime(& results[i]->sort_time);
          (void) strftime(buf, sizeof buf, "%c", t); /* locale appropriate time */
          printf("%s\t%s\n",results[i]->file_name, buf); 
        } else {
          printf("%s\t%d\n",results[i]->file_name,results[i]->sort_time);
        }
      }
  }
}

/* for qsort to sort the stack of files we made */
int
entry_compare(const void *e1p, const void *e2p)
{
  const ENTRY *e1 = *(ENTRY * const *)e1p;
  const ENTRY *e2 = *(ENTRY * const *)e2p;
  double diff;

  diff = (reverse) ? difftime(e1->sort_time, e2->sort_time)
                   : difftime(e2->sort_time, e1->sort_time);

  if(diff < 0)
    return -1;
  else if(diff > 0)
    return 1;
  else 
    return 0;
}
