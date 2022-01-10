#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "bf.h"
#include "hash_file.h"

#define RECORDS_NUM 1000 // you can change it if you want
#define GLOBAL_DEPT 2 // you can change it if you want
#define FILE_NAME "data.db"
#define FILE_NAME2 "studentsFile.db"

const char* names[] = {
  "Yannis",
  "Christofos",
  "Sofia",
  "Marianna",
  "Vagelis",
  "Maria",
  "Iosif",
  "Dionisis",
  "Konstantina",
  "Theofilos",
  "Giorgos",
  "Dimitris"
};

const char* surnames[] = {
  "Ioannidis",
  "Svingos",
  "Karvounari",
  "Rezkalla",
  "Nikolopoulos",
  "Berreta",
  "Koronis",
  "Gaitanis",
  "Oikonomou",
  "Mailis",
  "Michas",
  "Halatsis"
};

const char* cities[] = {
  "Athens",
  "San Francisco",
  "Los Angeles",
  "Amsterdam",
  "London",
  "New York",
  "Tokyo",
  "Hong Kong",
  "Munich",
  "Miami"
};

#define CALL_OR_DIE(call)     \
  {                           \
    HT_ErrorCode code = call; \
    if (code != HT_OK) {      \
      printf("Error\n");      \
      exit(code);             \
    }                         \
  }

int main() {
  BF_Init(LRU);
  
  //first file
  CALL_OR_DIE(HT_Init());

  int indexDesc, indexDesc2;
  CALL_OR_DIE(HT_CreateIndex(FILE_NAME, GLOBAL_DEPT));
  CALL_OR_DIE(HT_OpenIndex(FILE_NAME, &indexDesc));
  CALL_OR_DIE(HT_CreateIndex(FILE_NAME2, GLOBAL_DEPT));
  CALL_OR_DIE(HT_OpenIndex(FILE_NAME2, &indexDesc2));

  Record record;
  srand(time(NULL));
  int r;
  printf("Insert Entries for the first file\n");
  for (int id = 0; id < RECORDS_NUM; ++id) {
    // create a record
    record.id = id;
    r = rand() % 12;
    memcpy(record.name, names[r], strlen(names[r]) + 1);
    r = rand() % 12;
    memcpy(record.surname, surnames[r], strlen(surnames[r]) + 1);
    r = rand() % 10;
    memcpy(record.city, cities[r], strlen(cities[r]) + 1);

    CALL_OR_DIE(HT_InsertEntry(indexDesc, record));
  }

  printf("Insert Entries for the second file\n");
  for (int id = 0; id < RECORDS_NUM; ++id) {
    // create a record
    record.id = id;
    r = rand() % 12;
    memcpy(record.name, names[r], strlen(names[r]) + 1);
    r = rand() % 12;
    memcpy(record.surname, surnames[r], strlen(surnames[r]) + 1);
    r = rand() % 10;
    memcpy(record.city, cities[r], strlen(cities[r]) + 1);

    CALL_OR_DIE(HT_InsertEntry(indexDesc2, record));
  }

  CALL_OR_DIE(HT_CloseFile(indexDesc));

  int id = rand() % RECORDS_NUM;
  
  printf("RUN PrintAllEntries for 1st file for id = %d and NULL\n", id);
  CALL_OR_DIE(HT_OpenIndex(FILE_NAME, &indexDesc));
  CALL_OR_DIE(HT_PrintAllEntries(indexDesc, &id));
  CALL_OR_DIE(HT_PrintAllEntries(indexDesc, NULL));
  CALL_OR_DIE(HT_CloseFile(indexDesc));
  
  
  id = rand() % RECORDS_NUM;
  
  printf("RUN PrintAllEntries for 2nd file for id = %d and NULL\n", id);

  CALL_OR_DIE(HT_PrintAllEntries(indexDesc2, &id));
  CALL_OR_DIE(HT_PrintAllEntries(indexDesc2, NULL));

  CALL_OR_DIE(HT_CloseFile(indexDesc2));


  CALL_OR_DIE(HT_OpenIndex(FILE_NAME, &indexDesc));

  printf("Adding 10 more registries to the first file\n");
  
  for (id = RECORDS_NUM ; id <RECORDS_NUM + 10 ; id++){
    // create a record
    record.id = id;
    r = rand() % 12;
    memcpy(record.name, names[r], strlen(names[r]) + 1);
    r = rand() % 12;
    memcpy(record.surname, surnames[r], strlen(surnames[r]) + 1);
    r = rand() % 10;
    memcpy(record.city, cities[r], strlen(cities[r]) + 1);

    CALL_OR_DIE(HT_InsertEntry(indexDesc, record));
  }
  
  printf("Run PrintAllEntries for the new entries\n");
  
  for (id = RECORDS_NUM ; id <RECORDS_NUM + 10 ; id++){
    CALL_OR_DIE(HT_PrintAllEntries(indexDesc, &id));
  }

  CALL_OR_DIE(HT_CloseFile(indexDesc));

  BF_Close();
}
