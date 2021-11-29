/*
 * wordsearch.c
 *
 * Copyright 2021 Andy Alt <andy400-dev@yahoo.com>
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

// n * n grid
const int GRID_SIZE = 20; // n

const char HOST[] = "random-word-api.herokuapp.com";
const char PAGE[] = "word";
const char PROTOCOL[] = "http";

const char fill_char = '-';


// Most of the network code and fail function was pinched and adapted from
// https://www.lemoda.net/c/fetch-web-page/

/* Quickie function to test for failures. It is actually better to use
   a macro here, since a function like this results in unnecessary
   function calls to things like "strerror". However, not every
   version of C has variadic macros. */

static void fail (int test, const char * format, ...)
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

static int get_word (char *str)
{
  struct addrinfo hints, *res, *res0;
  int error;
  /* "s" is the file descriptor of the socket. */
  int s;

  memset (&hints, 0, sizeof (hints));
  /* Don't specify what type of internet connection. */
  hints.ai_family = PF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  error = getaddrinfo (HOST, PROTOCOL, & hints, & res0);
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

  freeaddrinfo(res0);

  /* "format" is the format of the HTTP request we send to the web
     server. */

  const char * format = "\
GET /%s HTTP/1.0\r\n\
Host: %s\r\n\
User-Agent: https://github.com/theimpossibleastronaut/wordsearch\r\n\
\r\n";

  /* "msg" is the request message that we will send to the
     server. */

  char msg[BUFSIZ];
  int status = snprintf (msg, BUFSIZ, format, PAGE, HOST);
  if (status >= BUFSIZ)
  {
    fputs ("snprintf failed.", stderr);
    exit (EXIT_FAILURE);
  }

  /* Send the request. */
  status = send (s, msg, strlen (msg), 0);

  /* Check it succeeded. The FreeBSD manual page doesn't mention
     whether "send" sets errno, but
     "http://pubs.opengroup.org/onlinepubs/009695399/functions/send.html"
     claims it does. */

  fail (status == -1, "send failed: %s\n", strerror (errno));

  /* Our receiving buffer. */
  char buf[BUFSIZ+10];
  /* Get "BUFSIZ" bytes from "s". */
  int bytes = recv (s, buf, BUFSIZ, 0);
  int r = close (s);

  if (bytes <= 0)
    return -1;

  if (r != 0)
  {
    fputs ("Error closing socket\n", stderr);
    return -1;
  }

  fail (bytes == -1, "%s\n", strerror (errno));
  /* Nul-terminate the string before printing. */
  buf[bytes] = '\0';

  const char open_bracket[] = "[\"";
  const char closed_bracket[] = "\"]";
  const char *str_not_found = "Expected '%s' not found in string\n";
  char *buf_start = strstr (buf, open_bracket);

  if (buf_start != NULL)
    buf_start += strlen (open_bracket);
  else
  {
    fprintf (stderr, str_not_found, open_bracket);
    return -1;
  }

  char *buf_end = strstr (buf_start, closed_bracket);
  if (buf_end != NULL)
    *buf_end = '\0';
  else
  {
    fprintf (stderr, str_not_found, closed_bracket);
    return -1;
  }

  strcpy (str, buf_start);
  return 0;
}


int check (const int row, const int col, const char puzzle[][GRID_SIZE], const char c)
{
  const int u = toupper (c);
  if (u == puzzle[row][col] || puzzle[row][col] == fill_char)
    return 0;

  return -1;
}

void place (const int row, const int col, char puzzle[][GRID_SIZE], const char c)
{
  puzzle[row][col] = toupper(c);
  return;
}

static int horizontal (const int len, const char *str, char puzzle[][GRID_SIZE])
{
  int col = rand() % (GRID_SIZE - len);
  const int row = rand() % GRID_SIZE;
  const int col_orig = col;
  char *ptr = (char *)str;
  while (*ptr != '\0')
  {
    if (check (row, col, puzzle, *ptr) == -1)
      return -1;

    ptr++;
    col++;
  }
  col = col_orig;
  ptr = (char *)str;
  while (*ptr != '\0')
  {
    place (row, col, puzzle, *ptr);
    ptr++;
    col++;
  }

  return 0;
}

static int horizontal_backward (const int len, const char *str, char puzzle[][GRID_SIZE])
{
  int col = (rand() % (GRID_SIZE - len)) + len ;
  const int row = rand () % GRID_SIZE;
  const int col_orig = col;
  char *ptr = (char *)str;
  while (*ptr != '\0')
  {
    if (check (row, col, puzzle, *ptr) == -1)
      return -1;
    ptr++;
    col--;
  }

  col = col_orig;
  ptr = (char *)str;
  while (*ptr != '\0')
  {
    place (row, col, puzzle, *ptr);
    ptr++;
    col--;
  }
  return 0;
}

static int vertical (const int len,  const char *str, char puzzle[][GRID_SIZE])
{
  int row = rand() % (GRID_SIZE - len);
  const int col = rand () % GRID_SIZE;
  const int row_orig = row;
  char *ptr = (char *)str;
  while (*ptr != '\0')
  {
    if (check (row, col, puzzle, *ptr) == -1)
      return -1;
    ptr++;
    row++;
  }

  row = row_orig;
  ptr = (char *)str;
  while (*ptr != '\0')
  {
    place (row, col, puzzle, *ptr);
    ptr++;
    row++;
  }
  return 0;
}

static int vertical_up (const int len, const char *str, char puzzle[][GRID_SIZE])
{
  int row = (rand() % (GRID_SIZE - len)) + len;
  const int col = rand () % GRID_SIZE;
  const int row_orig = row;
  char *ptr = (char *)str;
  while (*ptr != '\0')
  {
    if (check (row, col, puzzle, *ptr) == -1)
      return -1;
    ptr++;
    row--;
  }

  row = row_orig;
  ptr = (char *)str;
  while (*ptr != '\0')
  {
    place (row, col, puzzle, *ptr);
    ptr++;
    row--;
  }
  return 0;
}


static int diaganol_down_right (const int len, const char *str, char puzzle[][GRID_SIZE])
{
  int row = (rand() % (GRID_SIZE - len));
  int col = (rand() % (GRID_SIZE - len));
  const int row_orig = row;
  const int col_orig = col;
  char *ptr = (char *)str;
  while (*ptr != '\0')
  {
    if (check (row, col, puzzle, *ptr) == -1)
      return -1;
    ptr++;
    col++;
    row++;
  }

  col = col_orig;
  row = row_orig;
  ptr = (char *)str;
  while (*ptr != '\0')
  {
    place (row, col, puzzle, *ptr);
    ptr++;
    col++;
    row++;
  }
  return 0;
}

static int diaganol_down_left (const int len, const char *str, char puzzle[][GRID_SIZE])
{
  int col = (rand() % (GRID_SIZE - len)) + len;
  int row = (rand() % (GRID_SIZE - len));
  const int row_orig = row;
  const int col_orig = col;
  char *ptr = (char *)str;
  while (*ptr != '\0')
  {
    if (check (row, col, puzzle, *ptr) == -1)
      return -1;
    ptr++;
    col--;
    row++;
  }

  col = col_orig;
  row = row_orig;
  ptr = (char *)str;
  while (*ptr != '\0')
  {
    place (row, col, puzzle, *ptr);
    ptr++;
    col--;
    row++;
  }
  return 0;
}

static int diaganol_up_left (const int len, const char *str, char puzzle[][GRID_SIZE])
{
  //int col = (rand() % (GRID_SIZE - len)) + len;
  int col = (rand() % (GRID_SIZE - len)) + len ;
  int row = (rand() % (GRID_SIZE - len)) + len;
  const int row_orig = row;
  const int col_orig = col;
  char *ptr = (char *)str;
  while (*ptr != '\0')
  {
    if (check (row, col, puzzle, *ptr) == -1)
      return -1;
    ptr++;
    col--;
    row--;
  }

  col = col_orig;
  row = row_orig;
  ptr = (char *)str;
  while (*ptr != '\0')
  {
    place (row, col, puzzle, *ptr);
    ptr++;
    col--;
    row--;
  }
  return 0;
}

static int diaganol_up_right (const int len, const char *str, char puzzle[][GRID_SIZE])
{
  int col = rand() % (GRID_SIZE - len);
  int row = (rand() % (GRID_SIZE - len)) + len;
  const int row_orig = row;
  const int col_orig = col;
  char *ptr = (char *)str;
  while (*ptr != '\0')
  {
    if (check (row, col, puzzle, *ptr) == -1)
      return -1;
    ptr++;
    col++;
    row--;
  }

  col = col_orig;
  row = row_orig;
  ptr = (char *)str;
  while (*ptr != '\0')
  {
    place (row, col, puzzle, *ptr);
    ptr++;
    col++;
    row--;
  }
  return 0;
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

int main(int argc, char **argv)
{
  const int directions = 8;

  char puzzle[GRID_SIZE][GRID_SIZE];
  const int max_words_target = GRID_SIZE;
  char words[max_words_target][BUFSIZ];
  // const size_t n_max_words = sizeof(words)/sizeof(words[0]);

  int i = 0;
  int j = 0;

  for (i = 0; i < max_words_target; i++)
  {
    *words[i] = '\0';
  }

  /* initialize the puzzle with fill_char */
  for (i = 0; i < GRID_SIZE; i++)
  {
    for (j = 0; j < GRID_SIZE; j++)
    {
      puzzle[i][j] = fill_char;
    }
  }

  // Create an array of function pointers
  int (*direction_arr[])(const int, const char*, char(*)[GRID_SIZE]) = {
    horizontal,
    horizontal_backward,
    vertical,
    vertical_up,
    diaganol_down_left,
    diaganol_down_right,
    diaganol_up_left,
    diaganol_up_right
  };

  /* seed the random number generator */
  const long unsigned int seed = time(NULL);
  srand (seed);

  const int max_len = GRID_SIZE - 2;
  int n_string = 0;
  int tries = 0;
  // this probably means the word server is having issues. If this number is exceeded,
  // we'll quit completely
  const int max_tot_err_allowed = 10;
  int n_tot_err = 0;
  printf ("Attempting to fetch %d from %s...\n", max_words_target, HOST);
  while ((n_string < max_words_target) && n_tot_err < max_tot_err_allowed)
  {
    int t = 0;
    int r = -1;
    while (t < 3 && r == -1)
    {
      r = get_word (words[n_string]);
      t++;
      if (r != 0)
        n_tot_err++;
    }

    if (r != 0)
    {
      fputs ("Failed to get word from server\n", stderr);
      *words[n_string] = '\0';
      continue;
    }

    const int len = strlen (words[n_string]);
    if (len > max_len) // skip the word if it exceeds this value
    {
      printf ("word '%s' exceeded max length\n", words[n_string]);
      *words[n_string] = '\0';
      continue;
    }

    printf ("%d.) %s\n", n_string + 1, words[n_string]);

    const int max_tries = GRID_SIZE * 4;
    // The word will be skipped if a place can't be found
    for (tries = 0; tries < max_tries; tries++)
    {
      int rnd;
      // After n number of tries, try different directions.
      if (tries == 0 || tries > max_tries/2)
      {
        rnd = rand () % directions;
        // rnd = 7;
      }

      r = direction_arr[rnd] (len, words[n_string], puzzle);

      if (r == 0)
        break;
    }

    if (r == 0)
    {
      n_string++;
    }
    else
    {
      n_tot_err++;
      printf ("Unable to find a place for '%s'\n", words[n_string]);
      *words[n_string] = '\0';
    }
  }

  if (n_tot_err >= max_tot_err_allowed)
  {
    fprintf (stderr, "Too many errors (%d) communicating with server; giving up\n", n_tot_err);
    return -1;
  }

  puts (" ==] Answer key [==");
  for (i = 0; i < GRID_SIZE; i++)
  {
    for (j = 0; j < GRID_SIZE; j++)
    {
      printf ("%c ", puzzle[i][j]);
    }
    puts ("");
  }

  printf ("\n\n\n");

  /* Fill in any unused spots with random letters */
  for (i = 0; i < GRID_SIZE; i++)
  {
    for (j = 0; j < GRID_SIZE; j++)
    {
      if (puzzle[i][j] == fill_char)
        puzzle[i][j] = (rand () % ((int)'Z' - (int)'A' + 1)) + (int)'A';
    }
  }

  // print the puzzle
  for (i = 0; i < GRID_SIZE; i++)
  {
    for (j = 0; j < GRID_SIZE; j++)
    {
      printf ("%c ", puzzle[i][j]);
    }
    puts ("");
  }

  // print the words to search for
  puts ("");
  i = 0;
  while (i < max_words_target)
  {
    if (*words[i] != '\0')
      printf ("%*s", max_len + 1, words[i]);
    i++;

    // start a new row after every 3 words
    if (i % 3 == 0)
      puts ("");
  }
  puts ("");

  // write the seed and the fetched words to a log file
  if (argc > 1)
  {
    if (strcmp (argv[1], "-log") == 0)
    {
      char log_file[BUFSIZ];
      snprintf (log_file, BUFSIZ, "wordsearch_%lu.log", seed);
      FILE *fp = fopen (log_file, "w");
      if (fp != NULL)
      {
        fprintf (fp, "seed = %lu\n\n", seed);
        i = 0;
        while (i < max_words_target)
        {
          if (*words[i] != '\0')
            fprintf (fp, "%s\n", words[i]);
          i++;
        }
      }
      else
      {
        fputs ("Error while opening ", stderr);
        perror (log_file);
        return -1;
      }

      if (fclose (fp) != 0)
        fprintf (stderr, "Error closing %s\n", log_file);
    }
    else
    {
      printf ("invalid option: %s\n", argv[1]);
    }
  }

  return 0;
}

