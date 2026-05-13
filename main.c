#include "phonebook.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void populate_random(const char *pb_name) {

  srand((unsigned int)time(NULL));
  pb_init(pb_name);
  char n[64];
  strcpy(n, pb_name);
  strcat(n, ".pb");
  FILE *pb = fopen(n, "r+b");
  for (int i = 0; i < 10000; i++) {

    int id = rand() % 1000000;
    char first[32];
    char second[32];
    char number[32];

    snprintf(first, sizeof(first), "First%d", id);
    snprintf(second, sizeof(second), "Second%d", id);
    snprintf(number, sizeof(number), "+3725%07d", id);
    if (i == 9017) {
      printf("%s %s %s \n", first, second, number);
    }
    pb_add(first, second, number, pb);
  }
  puts("done");
  Record r;
  r = find_record_by_name("Timur", "Sirazitdinov", pb);
  printf("\n");
  printf("%s %s %s", r.first_name, r.second_name, r.number);
  return;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    return EXIT_FAILURE;
  }

  const char *cmd = argv[1];
  if (strcmp(cmd, "rnd") == 0) {
    const char *file_name = argv[2];
    populate_random(file_name);
  } else if (strcmp(cmd, "find") == 0) {

    FILE *pb = fopen(argv[4], "r+b");
    Record r = find_record_by_name(argv[2], argv[3], pb);
    printf("%s %s %s \n", r.first_name, r.second_name, r.number);
  } else if (strcmp(cmd, "add") == 0) {
    FILE *pb = fopen(argv[5], "r+b");
    bool res = pb_add(argv[2], argv[3], argv[4], pb);
    if (res) {
      puts("Success\n");
    } else {
      puts("Failure\n");
    }
  }
}
