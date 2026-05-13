#ifndef PHONEBOOK_H
#define PHONEBOOK_H

#include <stdbool.h>
#include <stdio.h>

#define PB_NAME_MAX 32
#define PB_SECOND_NAME_MAX 32
#define PB_NUMBER_MAX 32

typedef struct {
  char first_name[PB_NAME_MAX];
  char second_name[PB_SECOND_NAME_MAX];
  char number[PB_NUMBER_MAX];
  long left_child_offset;
  long right_child_offset;
  size_t height;
} Record;

typedef struct {
  long root_offset;
} PBHeader;

bool pb_init(const char *pb_name);

bool pb_add(const char *first_name, const char *second_name, const char *number,
            FILE *pb);

Record find_record_by_name(const char *first_name, const char *second_name,
                           FILE *pb);
#endif // !PHONEBOOK_H
