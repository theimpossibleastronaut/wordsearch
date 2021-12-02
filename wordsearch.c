/*
 * wordsearch.c
 *
 * Copyright 2021 Andy Alt <andy400-dev@yahoo.com>
 * https://github.com/theimpossibleastronaut/wordsearch
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <getopt.h>
#include <stdbool.h>

#define VERSION "0.1.1999"
#define PROGRAM_NAME "aawordsearch"
// n * n grid
const int GRID_SIZE = 20;       // n

const char HOST[] = "random-word-api.herokuapp.com";
const char PAGE[] = "word";
const char PROTOCOL[] = "http";

const char fill_char = '-';

enum
{
  HORIZONTAL,
  HORIZONTAL_BACKWARD,
  VERTICAL,
  VERTICAL_UP,
  DIAGANOL_DOWN_RIGHT,
  DIAGANOL_DOWN_LEFT,
  DIAGANOL_UP_RIGHT,
  DIAGANOL_UP_LEFT,
};

struct dir_op
{
  int begin_row;
  int begin_col;
  const int row;
  const int col;
  void (*direction)(struct dir_op *dir_op, const int len);
};

// Most of the network code and fail function was pinched and adapted from
// https://www.lemoda.net/c/fetch-web-page/

/* Quickie function to test for failures. It is actually better to use
   a macro here, since a function like this results in unnecessary
   function calls to things like "strerror". However, not every
   version of C has variadic macros. */

static void
fail (int test, const char *format, ...)
{
  if (test)
  {
    va_list args;
    va_start (args, format);
    vfprintf (stderr, format, args);
    va_end (args);
    // exit ok
    exit (EXIT_FAILURE);
  }
}


static int
get_words (char str[][BUFSIZ], const int fetch_count)
{
  struct addrinfo hints, *res, *res0;
  int error;
  /* "s" is the file descriptor of the socket. */
  int s;

  memset (&hints, 0, sizeof (hints));
  /* Don't specify what type of internet connection. */
  hints.ai_family = PF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  error = getaddrinfo (HOST, PROTOCOL, &hints, &res0);
  fail (error, gai_strerror (error));
  s = -1;
  for (res = res0; res; res = res->ai_next)
  {
    s = socket (res->ai_family, res->ai_socktype, res->ai_protocol);
    fail (s < 0, "socket: %s\n", strerror (errno));
    if (connect (s, res->ai_addr, res->ai_addrlen) < 0)
    {
      fprintf (stderr, "connect: %s\n", strerror (errno));
      close (s);
      exit (EXIT_FAILURE);
    }
    break;
  }

  freeaddrinfo (res0);

  /* "format" is the format of the HTTP request we send to the web
     server. */

  const char *format = "\
GET /%s?number=%d HTTP/1.0\r\n\
Host: %s\r\n\
User-Agent: github.com/theimpossibleastronaut/aawordsearch (v%s)\r\n\
\r\n";

  /* "msg" is the request message that we will send to the
     server. */

  char msg[BUFSIZ];
  int status = snprintf (msg, BUFSIZ, format, PAGE, fetch_count, HOST, VERSION);
  if (status >= BUFSIZ)
  {
    fputs ("snprintf failed.", stderr);
    return -1;
  }

  /* Send the request. */
  status = send (s, msg, strlen (msg), 0);

  /* Check it succeeded. The FreeBSD manual page doesn't mention
     whether "send" sets errno, but
     "http://pubs.opengroup.org/onlinepubs/009695399/functions/send.html"
     claims it does. */

  fail (status == -1, "send failed: %s\n", strerror (errno));

  /* Our receiving buffer. */
  char srv_str[BUFSIZ + 10];
  *srv_str = '\0';
  char buf[BUFSIZ + 10];
  *buf = '\0';
  int bytes;
  do
  {
    /* Get "BUFSIZ" bytes from "s". */
    bytes = recv (s, srv_str, BUFSIZ, 0);
    fail (bytes == -1, "%s\n", strerror (errno));

    /* Nul-terminate the string before printing. */
    // srv[bytes] = '\0';
    int max_len = BUFSIZ - strlen (buf);
    // concatenate the string each iteration of the loop
    status = snprintf (buf + strlen (buf), max_len, "%s", srv_str);
    if (status >= max_len)
    {
      fputs ("snprintf failed.", stderr);
      return -1;
    }
  }
  while (bytes > 0);

  if (close (s) != 0)
  {
    fputs ("Error closing socket\n", stderr);
    return -1;
  }

  const char open_bracket[] = "[\"";
  const char closed_bracket[] = "\"]";
  const char *str_not_found = "Expected '%s' not found in string\n";
  char *buf_start = strstr (buf, open_bracket);

  if (buf_start != NULL)
    buf_start++;
  else
  {
    fprintf (stderr, str_not_found, open_bracket);
    return -1;
  }

  char *buf_end = strstr (buf_start, closed_bracket);
  if (buf_end != NULL)
    *buf_end = '\0'; // replaces the ']' so the string should end with '"'
  else
  {
    fprintf (stderr, str_not_found, closed_bracket);
    return -1;
  }

  const char delimiter[] = "\",\"";
  char *token = strtok (buf_start, delimiter);
  int n_word = 0;

  while (token != NULL)
  {
    // printf("[%s]\n", token);
    strcpy (str[n_word], token);
    token = strtok(NULL, delimiter);
    n_word++;
  }

  return 0;
}


int
check (const int row, const int col, const char puzzle[][GRID_SIZE],
       const char c)
{
  const int u = toupper (c);
  if (u == puzzle[row][col] || puzzle[row][col] == fill_char)
    return 0;

  return -1;
}


static int
direction (struct dir_op *dir_op, const int len, const char *str,
           char puzzle[][GRID_SIZE])
{
  int row = dir_op->begin_row;
  int col = dir_op->begin_col;
  char *ptr = (char *) str;
  while (*ptr != '\0')
  {
    if (check (row, col, puzzle, *ptr) == -1)
      return -1;
    ptr++;
    col += dir_op->col;
    row += dir_op->row;
  }

  row = dir_op->begin_row;
  col = dir_op->begin_col;
  ptr = (char *) str;
  while (*ptr != '\0')
  {
    puzzle[row][col] = toupper (*ptr);
    ptr++;
    col += dir_op->col;
    row += dir_op->row;
  }
  return 0;
}


void
print_answer_key (FILE * restrict stream, const char puzzle[][GRID_SIZE])
{
  int i, j;
  fputs (" ==] Answer key [==\n", stream);
  for (i = 0; i < GRID_SIZE; i++)
  {
    for (j = 0; j < GRID_SIZE; j++)
    {
      fprintf (stream, "%c ", puzzle[i][j]);
    }
    fputs ("\n", stream);
  }
  fputs ("\n\n\n", stream);
  return;
}


void
print_puzzle (FILE * restrict stream, const char puzzle[][GRID_SIZE])
{
  int i, j;
  for (i = 0; i < GRID_SIZE; i++)
  {
    for (j = 0; j < GRID_SIZE; j++)
    {
      if (puzzle[i][j] == fill_char)
        fprintf (stream, "%c ",
                 (rand () % ((int) 'Z' - (int) 'A' + 1)) + (int) 'A');
      else
        fprintf (stream, "%c ", puzzle[i][j]);
    }
    fputs ("\n", stream);
  }
  fputs ("\n", stream);
  return;
}


static void
print_words (FILE * restrict stream, const char words[][BUFSIZ],
             const char puzzle[][GRID_SIZE], const int n_string,
             const int max_len)
{
  int i = 0;
  while (i < n_string)
  {
    if (*words[i] != '\0')
      fprintf (stream, "%*s", max_len + 1, words[i]);
    i++;

    // start a new row after every 3 words
    if (i % 3 == 0)
      fputs ("\n", stream);
  }
  fputs ("\n", stream);
  return;
}

//struct dir_test
//{

//};

static void
horizontal (struct dir_op *dir_op, const int len)
{
  dir_op->begin_row = rand () % GRID_SIZE;
  dir_op->begin_col = rand () % (GRID_SIZE - len);
  return;
}


static void
horizontal_left (struct dir_op *dir_op, const int len)
{
  dir_op->begin_row = rand () % GRID_SIZE;
  dir_op->begin_col = (rand () % (GRID_SIZE - len)) + len;
  return;
}


static void
vertical (struct dir_op *dir_op, const int len)
{
  dir_op->begin_row = rand () % (GRID_SIZE - len);
  dir_op->begin_col = rand () % GRID_SIZE;
  return;
}


static void
vertical_up (struct dir_op *dir_op, const int len)
{
  dir_op->begin_row = (rand () % (GRID_SIZE - len)) + len;
  dir_op->begin_col = rand () % GRID_SIZE;
  return;
}


static void
diaganol_down_right (struct dir_op *dir_op, const int len)
{
  dir_op->begin_row = (rand () % (GRID_SIZE - len));
  dir_op->begin_col = (rand () % (GRID_SIZE - len));
  return;
}


static void
diaganol_down_left (struct dir_op *dir_op, const int len)
{
  dir_op->begin_row = (rand () % (GRID_SIZE - len));
  dir_op->begin_col = (rand () % (GRID_SIZE - len)) + len;
  return;
}


static void
diaganol_up_right (struct dir_op *dir_op, const int len)
{
  dir_op->begin_row = (rand () % (GRID_SIZE - len)) + len;
  dir_op->begin_col = rand () % (GRID_SIZE - len);
  return;
}


static void
diaganol_up_left (struct dir_op *dir_op, const int len)
{
  dir_op->begin_row = (rand () % (GRID_SIZE - len)) + len;
  dir_op->begin_col = (rand () % (GRID_SIZE - len)) + len;
  return;
}


/* TODO: To not punish the word server, use this for debugging */
//const char *words[] = {
  //"received",
  //"software",
  //"purpose",
  //"version",
  //"program",
  //"foundation",
  //"General",
  //"Public",
  //"License",
  //"distributed",
  //"California",
  //"Mainland",
  //"Honeymoon",
  //"simpson",
  //"incredible",
  //"MERCHANTABILITYMERCHANTABILITY",
  //"warranty",
  //"temperate",
  //"london",
  //"tremendous",
  //"desperate",
  //"paradoxical",
  //"starship",
  //"enterprise",
  //NULL
//};

// const size_t n_strings = sizeof(strings)/sizeof(strings[0]);
void
print_usage()
{
  printf ("\n\
  -h, --help                show help for command line options\n\
  -V, --version             show the program version number\n\
  -l, --list                list waste directories\n");
}

int
main (int argc, char **argv)
{
  const int directions = 8;
  char puzzle[GRID_SIZE][GRID_SIZE];
  const int max_words_target = GRID_SIZE;
  const int fetch_count = max_words_target * 1.4;
  char fetched_words[fetch_count][BUFSIZ];
  // const size_t n_max_words = sizeof(words)/sizeof(words[0]);

  bool log = false;

  const struct option long_options[] = {
    {"help", no_argument, NULL, 'h'},
    {"log", no_argument, NULL, 'l'},
    {"version", no_argument, NULL, 'V'},
    {0, 0, 0, 0}
  };

  int option_index = 0;
  int c;
  while ((c = getopt_long (argc, argv, "hlV", long_options, &option_index)) != -1)
  {
    switch ((char)c)
    {
    case 'h':
      print_usage ();
      return 0;
    case 'l':
      log = true;
      puts ("The log will be activated! Hooray!");
      break;
    case 'V':
      printf ("%s v%s\n\n", PROGRAM_NAME, VERSION);
      return 0;
    case '?':
      printf ("Try '%s --help' for more information.\n", argv[0]);
      return -1;
    default:
      printf("?? getopt returned character code 0%o ??\n", c);
    }
  }

  /* Print any remaining command line arguments (not options). */
  if (optind < argc)
  {
    printf ("non-option ARGV-elements: ");
    while (optind < argc)
      printf ("%s ", argv[optind++]);
    putchar ('\n');
  }

  /* initialize the puzzle with fill_char */
  int i = 0;
  int j = 0;
  for (i = 0; i < GRID_SIZE; i++)
  {
    for (j = 0; j < GRID_SIZE; j++)
    {
      puzzle[i][j] = fill_char;
    }
  }

  /* seed the random number generator */
  const long unsigned int seed = time (NULL);
  srand (seed);

  const int max_len = GRID_SIZE - 2;
  int tries = 0;
  // this probably means the word server is having issues. If this number is exceeded,
  // we'll quit completely
  const int max_tot_err_allowed = 10;
  int n_tot_err = 0;
  printf ("Attempting to fetch %d words from %s...\n", max_words_target,
          HOST);
  int t = 0;
  int r;
  do
  {
    r = get_words (fetched_words, fetch_count);
    if (r != 0)
      n_tot_err++;
  }
  while (++t < 3 && r == -1);

  if (r != 0)
  {
    fputs ("Failed to get words from server\n", stderr);
    return -1;
  }

  char words[max_words_target][BUFSIZ];
  for (i = 0; i < max_words_target; i++)
  {
    *words[i] = '\0';
  }

  // Make an array of function pointers
  struct dir_op dir_op[] = {
    {0, 0, 0, 1, horizontal},
    {0, 0, 0, -1, horizontal_left},
    {0, 0, 1, 0, vertical},
    {0, 0, -1, 0, vertical_up},
    {0, 0, 1, 1, diaganol_down_right},
    {0, 0, 1, -1, diaganol_down_left},
    {0, 0, -1, 1, diaganol_up_right},
    {0, 0, -1, -1, diaganol_up_left}
  };

  int n_string = 0;
  int f_string = 0;
  while ((n_string < max_words_target) && n_tot_err < max_tot_err_allowed)
  {
    strcpy (words[n_string], fetched_words[f_string]);
    const int len = strlen (words[n_string]);
    if (len > max_len)          // skip the word if it exceeds this value
    {
      printf ("word '%s' exceeded max length\n", words[n_string]);
      *words[n_string] = '\0';
      f_string++;
      continue;
    }

    printf ("%d.) %s\n", n_string + 1, words[n_string]);

    const int max_tries = GRID_SIZE * 4;
    int cur_dir = 0;
    // The word will be skipped if a place can't be found
    for (tries = 0; tries < max_tries; tries++)
    {
      // int rnd;
      // After n number of tries, try different directions.
      if (max_tries / 2)
      {
        cur_dir = rand () % directions;
        //rnd = 1;
      }

      dir_op[cur_dir].direction (&dir_op[cur_dir], len);
      r = direction (&dir_op[cur_dir], len, words[n_string], puzzle);
      if (r == 0)
        break;
    }

    if (r == 0)
    {
      n_string++;
      f_string++;
      cur_dir > (directions - 1) ? cur_dir = 0 : cur_dir++;
    }
    else
    {
      n_tot_err++;
      printf ("Unable to find a place for '%s'\n", words[n_string]);
      f_string++;
    }

    if (n_tot_err >= max_tot_err_allowed)
    {
      fprintf (stderr,
               "Too many errors (%d) communicating with server; giving up\n",
               n_tot_err);
      return -1;
    }
  }

  print_answer_key (stdout, puzzle);
  print_puzzle (stdout, puzzle);
  print_words (stdout, words, puzzle, n_string, max_len);

  // write the seed, answer key, and puzzle to a file
  if (log)
  {
    char log_file[BUFSIZ];
    snprintf (log_file, BUFSIZ, "wordsearch_%lu.log", seed);
    FILE *fp = fopen (log_file, "w");
    if (fp != NULL)
    {
      fprintf (fp, "seed = %lu\n\n", seed);
      print_answer_key (fp, puzzle);
      print_puzzle (fp, puzzle);
      print_words (fp, words, puzzle, max_words_target, max_len);
    }
    else
    {
      fputs ("Error while opening ", stderr);
      perror (log_file);
      return -1;
    }

    if (fclose (fp) != 0)
      fprintf (stderr, "Error closing %s\n", log_file);

    char word_log_file[BUFSIZ];
    snprintf (word_log_file, BUFSIZ, "wordsearch_words_%lu.log", seed);
    fp = fopen (word_log_file, "w");
    if (fp != NULL)
    {
      int l = 0;
      while (l < n_string)
      {
        fprintf (fp, "%s\n", words[l]);
        l++;
      }
    }
    if (fclose (fp) != 0)
      fprintf (stderr, "Error closing %s\n", log_file);
  }

  return 0;
}
