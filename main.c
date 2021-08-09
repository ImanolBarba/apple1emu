/***************************************************************************
 *   main.c  --  This file is part of apple1emu.                           *
 *                                                                         *
 *   Copyright (C) 2021 Imanol-Mikel Barba Sabariego                       *
 *                                                                         *
 *   apple1emu is free software: you can redistribute it and/or modify     *
 *   it under the terms of the GNU General Public License as published     *
 *   by the Free Software Foundation, either version 3 of the License,     *
 *   or (at your option) any later version.                                *
 *                                                                         *
 *   apple1emu is distributed in the hope that it will be useful,          *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty           *
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.               *
 *   See the GNU General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see http://www.gnu.org/licenses/.   *
 *                                                                         *
 ***************************************************************************/

#include "apple1.h"
#include "errors.h"

#include <stdio.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>

#define VERSION 1

// TODO: Make relevant errors use perror

static struct option long_options[] = {
  {"help",  no_argument,       NULL, 'h'},
  {"memory",  required_argument, NULL, 'm'},
  {"extra",  required_argument, NULL, 'e'},
  {"rom",  required_argument, NULL, 'r'},
  {"perf-counter", required_argument, NULL, 'p'},
  {NULL, 0, NULL, 0}
};

void termination_handler (int signum) {
  if(signum == SIGINT) {
    halt_apple1();
  }
}

void print_version(const char* argv) {
    printf("%s v%d\n\n", argv, VERSION);
}

void print_help(const char* argv) {
  print_version(argv);
  printf("%s [-r --rom ROM_PATH] [-e --extra EXTRA_RAM_PATH] [-m --mem USER_MEMORY_SIZE] [-p --perf-counter PERF_COUNTER_FREQ] [-h --help]\n", argv);
  printf("Just a simple Apple I emulator.\n\n");
}

size_t load_file(const char* path, uint8_t** dest) {
  struct stat st;
  stat(path, &st);
  size_t file_size = st.st_size;
  int fd = open(path, O_RDONLY);
  if(fd == -1) {
    fprintf(stderr, "Error opening file: %s\n", path);
    return ERROR_OPEN_FILE;
  }
  uint8_t* data = malloc(file_size);
  if(data == NULL) {
    fprintf(stderr, "Unable to allocate memory for file");
    close(fd);
    return ERROR_MEMORY_ALLOC;
  }
  ssize_t num_read = 0;
  size_t data_pos = 0;
  while(num_read != file_size) {
    num_read = read(fd, data + data_pos, file_size);
    if(num_read == -1) {
      if(errno != EINTR) {
        fprintf(stderr, "Error reading file\n");
        free(data);
        close(fd);
        return ERROR_READ_FILE;
      }
    } else {
      data_pos += num_read;
    }
  }
  close(fd);

  *dest = data;

  return file_size;
}

int main(int argc, char** argv) {
  char* rom_path = NULL;
  char* extra_path = NULL;
  size_t user_memory_size = 0xB000;

  uint8_t* rom_data = NULL;
  size_t rom_length = 0;
  uint8_t* extra_data = NULL;
  size_t extra_length = 0;

  unsigned int perf_counter_frequency = DEFAULT_PERF_COUNTER_FREQ;

  struct sigaction act;
  memset(&act, 0, sizeof(act));

  /* Set up the structure to specify the new action. */
  act.sa_handler = termination_handler;
  sigemptyset(&act.sa_mask);

  if(sigaction(SIGINT, &act, NULL) == -1) {
		fprintf(stderr, "Unable to set SIGINT handler");
		exit(FAILURE);
	}

  int c;
  int option_index;
  while ((c = getopt_long(argc, argv, "hm:e:r:p:", long_options, &option_index)) != -1) {
    switch (c) {
      case 'm':
        user_memory_size = atoi(optarg);
      break;
      case 'e':
        extra_path = optarg;
      break;
      case 'r':
        rom_path = optarg;
      break;
      case 'p':
        perf_counter_frequency = atoi(optarg);
      break;
      case 'h':
      case '?':
        print_help(argv[0]);
        exit(SUCCESS);
      break;
      default:
        fprintf(stderr, "Invalid getopt result\n");
        exit(FAILURE);
      break;
    }
  }

  if(rom_path == NULL) {
    fprintf(stderr, "Missing argument: -r\n");
    exit(FAILURE);
  }
  rom_length = load_file(rom_path, &rom_data);
  if(rom_length < 0) {
    exit(FAILURE);
  }

  if(extra_path == NULL) {
    fprintf(stderr, "Missing argument: -e\n");
    exit(FAILURE);
  }
  extra_length = load_file(extra_path, &extra_data);
  if(extra_length < 0) {
    exit(FAILURE);
  }

  init_apple1(user_memory_size, rom_data, rom_length, extra_data, extra_length);
  int ret = boot_apple1(perf_counter_frequency);
  if(ret != SUCCESS) {
    exit(FAILURE);
  }
  fprintf(stderr, "Halted apple I\n");

  free(rom_data);
  free(extra_data);

  return 0;
}
