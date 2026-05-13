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
  pb = fopen(new_name, "wb");
  if (pb == NULL) {
    return false;
  }
  PBHeader header = {.root_offset = -1};
  fwrite(&header, sizeof(header), 1, pb);
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

static long insert_record_at_current_pos(FILE *pb, Record *record) {
  long offset = ftell(pb);
  fwrite(record, sizeof(Record), 1, pb);
  return offset;
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

static size_t get_max(size_t h1, size_t h2) {
  if (h1 > h2) {
    return h1;
  }

  if (h2 > h1) {
    return h2;
  }

  return h1;
}

static void overwrite_height(FILE *pb, size_t new_height) {
  long current_pos = ftell(pb);
  fseek(pb, 96 + 2 * sizeof(long), SEEK_CUR);
  fwrite(&new_height, sizeof(size_t), 1, pb);
  fseek(pb, current_pos, SEEK_SET);
}

static int get_height_of_current_node(FILE *pb) {
  long current_pos = ftell(pb);
  fseek(pb, 96 + 2 * sizeof(long), SEEK_CUR);
  size_t height;
  fread(&height, sizeof(size_t), 1, pb);
  fseek(pb, current_pos, SEEK_SET);
  return height;
}

static size_t get_height_of_left_child(long left_child_offset, FILE *pb) {
  if (left_child_offset == -1) {
    return 0;
  }
  long return_to = ftell(pb);
  fseek(pb, left_child_offset, SEEK_SET);
  size_t height = get_height_of_current_node(pb);
  fseek(pb, return_to, SEEK_SET);
  return height;
}

static size_t get_height_of_right_child(long right_child_offset, FILE *pb) {
  if (right_child_offset == -1) {
    return 0;
  }
  long return_to = ftell(pb);
  fseek(pb, right_child_offset, SEEK_SET);
  size_t height = get_height_of_current_node(pb);
  fseek(pb, return_to, SEEK_SET);
  return height;
}

static int get_balance(FILE *pb, long return_to, Record *record) {
  long left_child = record->left_child_offset;
  long right_child = record->right_child_offset;

  if (left_child == -1) {
    return -1 * get_height_of_right_child(right_child, pb);
  } else if (right_child == -1) {
    return get_height_of_left_child(left_child, pb);
  }

  int left_child_height;
  int right_child_height;

  left_child_height = (int)get_height_of_left_child(left_child, pb);
  right_child_height = (int)get_height_of_right_child(right_child, pb);

  return left_child_height - right_child_height;
}

static long right_rotation(FILE *pb, Record *record) {
  long initial_offset = ftell(pb);

  Record root = get_record_at_current_pos(pb);
  long left_child = root.left_child_offset;

  fseek(pb, left_child, SEEK_SET);
  Record left = get_record_at_current_pos(pb);
  long t2 = left.right_child_offset;

  overwrite_right_child(initial_offset, pb);
  fseek(pb, initial_offset, SEEK_SET);

  overwrite_left_child(t2, pb);

  Record current = get_record_at_current_pos(pb);
  long l = current.left_child_offset;
  long r = current.right_child_offset;

  overwrite_height(pb, 1 + get_max(get_height_of_left_child(l, pb),
                                   get_height_of_right_child(r, pb)));
  fseek(pb, left_child, SEEK_SET);

  current = get_record_at_current_pos(pb);
  l = current.left_child_offset;
  r = current.right_child_offset;
  overwrite_height(pb, 1 + get_max(get_height_of_left_child(l, pb),
                                   get_height_of_right_child(r, pb)));

  return left_child;
}

static long left_rotation(FILE *pb, Record *record) {
  long offset = ftell(pb);
  long right_immediate_child = record->right_child_offset;
  fseek(pb, right_immediate_child, SEEK_SET);
  Record t2 = get_record_at_current_pos(pb);
  overwrite_left_child(offset, pb);
  Record r = get_record_at_current_pos(pb);

  fseek(pb, offset, SEEK_SET);
  Record r2 = get_record_at_current_pos(pb);
  overwrite_right_child(t2.left_child_offset, pb);
  r2 = get_record_at_current_pos(pb);
  overwrite_height(
      pb, 1 + get_max(get_height_of_left_child(r2.left_child_offset, pb),
                      get_height_of_right_child(r2.right_child_offset, pb)));

  fseek(pb, right_immediate_child, SEEK_SET);
  r = get_record_at_current_pos(pb);
  overwrite_height(
      pb, 1 + get_max(get_height_of_left_child(r.left_child_offset, pb),
                      get_height_of_right_child(r.right_child_offset, pb)));
  fseek(pb, offset, SEEK_SET);
  return right_immediate_child;
}

static long insert_record(Record *record, FILE *pb, bool *insert_success) {
  long current_offset = ftell(pb);
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
  } else {

    *insert_success = false;
    return current_offset;
  }
  current_record = get_record_at_current_pos(pb);
  size_t new_height =
      1 +
      get_max(get_height_of_left_child(current_record.left_child_offset, pb),
              get_height_of_right_child(current_record.right_child_offset, pb));

  overwrite_height(pb, new_height);
  current_record = get_record_at_current_pos(pb);

  int balance = get_balance(pb, current_offset, &current_record);

  if (balance > 1) {
    fseek(pb, current_record.left_child_offset, SEEK_SET);
    Record left_node = get_record_at_current_pos(pb);
    fseek(pb, current_offset, SEEK_SET);
    long res = compare_records(record, &left_node);
    if (res < 0) {
      return right_rotation(pb, &current_record);
    }
    if (res > 0) {
      fseek(pb, current_record.left_child_offset, SEEK_SET);
      long rot_res = left_rotation(pb, &left_node);
      fseek(pb, current_offset, SEEK_SET);
      overwrite_left_child(rot_res, pb);
      return right_rotation(pb, &current_record);
    }
  } else if (balance < -1) {
    fseek(pb, current_record.right_child_offset, SEEK_SET);
    Record right_node = get_record_at_current_pos(pb);
    fseek(pb, current_offset, SEEK_SET);
    long res = compare_records(record, &right_node);
    if (res < 0) {
      fseek(pb, current_record.right_child_offset, SEEK_SET);
      long rot_res = right_rotation(pb, &right_node);
      fseek(pb, current_offset, SEEK_SET);
      overwrite_right_child(rot_res, pb);
      return left_rotation(pb, &current_record);
    }
    if (res > 0) {
      return left_rotation(pb, &current_record);
    }
  }

  return current_offset;
}

static bool pb_empty(FILE *pb) {
  fseek(pb, 0, SEEK_END);
  long size = ftell(pb);
  rewind(pb);
  return size == 0;
}

static long get_root_offset(FILE *pb) {
  fseek(pb, 0, SEEK_SET);
  PBHeader header;
  fread(&header, sizeof(header), 1, pb);
  return header.root_offset;
}

static void overwrite_root_offset(long new_offset, FILE *pb) {
  fseek(pb, 0, SEEK_SET);
  fwrite(&new_offset, sizeof(new_offset), 1, pb);
}

bool pb_add(const char *first_name, const char *second_name, const char *number,
            FILE *pb) {
  Record record = pb_create_record(first_name, second_name, number);
  long root_offset = get_root_offset(pb);
  if (root_offset == -1) {
    root_offset = insert_record_at_current_pos(pb, &record);
    overwrite_root_offset(root_offset, pb);
    return true;
  }
  fseek(pb, root_offset, SEEK_SET);
  bool insert_success;
  long off = insert_record(&record, pb, &insert_success);
  overwrite_root_offset(off, pb);
  return insert_success;
}

Record find_record_by_name(const char *first_name, const char *second_name,
                           FILE *pb) {
  char full_name[64];
  strcpy(full_name, first_name);
  strcat(full_name, second_name);
  long root = get_root_offset(pb);
  fseek(pb, root, SEEK_SET);
  Record r;
  find_record_by_full_name(&r, full_name, pb);
  fseek(pb, 0, SEEK_SET);
  return r;
}
