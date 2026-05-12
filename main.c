#include "phonebook.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void) {
  srand((unsigned int)time(NULL));
  pb_init("test");
  FILE *pb = fopen("test.pb", "w+b");
  for (int i = 0; i < 1000; i++) {

    int id = rand() % 1000000;
    char first[32];
    char second[32];
    char number[32];
    if (i == 838) {

      snprintf(first, sizeof(first), "Timur");
      snprintf(second, sizeof(second), "Sirazitdinov");
      snprintf(number, sizeof(number), "53445506");
    } else {

      snprintf(first, sizeof(first), "First%d", id);
      snprintf(second, sizeof(second), "Second%d", id);
      snprintf(number, sizeof(number), "+3725%07d", id);
    }

    pb_add(first, second, number, pb);
  }
  puts("done");
  Record r;
  r = find_record_by_name("Timur", "Sirazitdinov", pb);
  printf("\n");
  printf("%s %s %s", r.first_name, r.second_name, r.number);
  return EXIT_SUCCESS;
}
