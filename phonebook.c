#include "phonebook.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static bool pb_exists(const char *pb_name) {
  return access(pb_name, F_OK) == 0;
}

bool pb_init(const char *pb_name) {

  size_t len = strlen(pb_name);
  char new_name[len + 4];
  strcpy(new_name, pb_name);
  strcat(new_name, ".pb");

  if (pb_exists(new_name)) {
    return false;
  }

  FILE *pb;
  pb = fopen(new_name, "w");
  if (pb == NULL) {
    return false;
  }
  fclose(pb);
  return true;
}

static Record pb_create_record(const char *first_name, const char *second_name,
                               const char *number) {
  Record record;
  strlcpy(record.first_name, first_name, 32);
  strlcpy(record.second_name, second_name, 32);
  strlcpy(record.number, number, 32);
  record.left_child_offset = -1;
  record.right_child_offset = -1;
  record.height = 1;

  return record;
}

static Record get_record_at_current_pos(FILE *pb) {
  Record record;
  fread(&record, sizeof(Record), 1, pb);
  fseek(pb, sizeof(Record) * -1, SEEK_CUR);
  return record;
}

static void insert_record_at_current_pos(FILE *pb, Record *record) {
  fwrite(record, sizeof(Record), 1, pb);
}

static void get_full_name(Record *record, char out[64]) {
  strlcpy(out, record->first_name, 32);
  strlcat(out, record->second_name, 32);
  return;
}

static int compare_records(Record *r1, Record *r2) {
  char r1_full_name[64];
  char r2_full_name[64];

  get_full_name(r1, r1_full_name);
  get_full_name(r2, r2_full_name);

  return strcmp(r1_full_name, r2_full_name);
}

static int compare_record_to_full_name(Record *record,
                                       const char full_name[64]) {
  char record_full_name[64];
  get_full_name(record, record_full_name);
  return strcmp(record_full_name, full_name);
}

static long append_record_and_return(Record *record, long return_to, FILE *pb) {
  fseek(pb, 0, SEEK_END);
  long offset = ftell(pb);
  insert_record_at_current_pos(pb, record);
  fseek(pb, return_to, SEEK_SET);
  return offset;
}

static void move_to_left_child(FILE *pb) {
  fseek(pb, 96, SEEK_CUR);
  return;
}

static void move_to_right_child(FILE *pb) {
  fseek(pb, 96 + sizeof(long), SEEK_CUR);
  return;
}

static void overwrite_left_child(long new_offset, FILE *pb) {
  // use offsetof
  move_to_left_child(pb);
  fwrite(&new_offset, sizeof(new_offset), 1, pb);
  fseek(pb, (96 + sizeof(new_offset)) * -1, SEEK_CUR);
}

static void overwrite_right_child(long new_offset, FILE *pb) {
  // use offsetof
  move_to_right_child(pb);
  fwrite(&new_offset, sizeof(new_offset), 1, pb);
  fseek(pb, (96 + sizeof(long) + sizeof(new_offset)) * -1, SEEK_CUR);
}

static long insert_record(Record *record, FILE *pb, bool *insert_success) {
  int current_offset = ftell(pb);
  Record current_record = get_record_at_current_pos(pb);
  int comp_res = compare_records(record, &current_record);

  if (comp_res < 0) {
    long left_child_offset = current_record.left_child_offset;
    if (left_child_offset == -1) {
      overwrite_left_child(append_record_and_return(record, current_offset, pb),
                           pb);
      *insert_success = true;
    } else {
      fseek(pb, left_child_offset, SEEK_SET);
      long new_offset = insert_record(record, pb, insert_success);
      fseek(pb, current_offset, SEEK_SET);
      overwrite_left_child(new_offset, pb);
    }
    return current_offset;
  } else if (comp_res > 0) {
    long right_child_offset = current_record.right_child_offset;
    if (right_child_offset == -1) {
      overwrite_right_child(
          append_record_and_return(record, current_offset, pb), pb);
      *insert_success = true;
    } else {
      fseek(pb, right_child_offset, SEEK_SET);
      long new_offset = insert_record(record, pb, insert_success);
      fseek(pb, current_offset, SEEK_SET);
      overwrite_right_child(new_offset, pb);
    }
    return current_offset;
  }

  *insert_success = false;
  return current_offset;
}

static bool pb_empty(FILE *pb) {
  fseek(pb, 0, SEEK_END);
  long size = ftell(pb);
  rewind(pb);
  return size == 0;
}

bool pb_add(const char *first_name, const char *second_name, const char *number,
            FILE *pb) {
  Record record = pb_create_record(first_name, second_name, number);
  if (pb_empty(pb)) {
    insert_record_at_current_pos(pb, &record);
    return true;
  }

  bool insert_success;
  insert_record(&record, pb, &insert_success);
  return insert_success;
}

static void get_number_of_current_record(FILE *pb, char *number) {
  long current_offset = ftell(pb);
  fseek(pb, 64, SEEK_CUR);
  char record_numb[64];
  fread(record_numb, sizeof(char), 32, pb);
  strcpy(number, record_numb);
  fseek(pb, current_offset, SEEK_SET);
  return;
}

static bool find_record_by_full_name(Record *record, const char *full_name,
                                     FILE *pb) {

  Record current_record = get_record_at_current_pos(pb);
  int comp_res = compare_record_to_full_name(&current_record, full_name);

  if (comp_res == 0) {
    *record = current_record;
    return true;
  } else if (comp_res > 0) {
    long left_child_offset = current_record.left_child_offset;
    if (left_child_offset == -1) {
      return false;
    } else {
      fseek(pb, left_child_offset, SEEK_SET);
      return find_record_by_full_name(record, full_name, pb);
    }
  } else {
    long right_child_offset = current_record.right_child_offset;
    if (right_child_offset == -1) {
      return false;
    } else {
      fseek(pb, right_child_offset, SEEK_SET);
      return find_record_by_full_name(record, full_name, pb);
    }
  }
}

Record find_record_by_name(const char *first_name, const char *second_name,
                           FILE *pb) {
  char full_name[64];
  strcpy(full_name, first_name);
  strcat(full_name, second_name);
  fseek(pb, 0, SEEK_SET);
  Record r;
  find_record_by_full_name(&r, full_name, pb);
  fseek(pb, 0, SEEK_SET);
  return r;
}
