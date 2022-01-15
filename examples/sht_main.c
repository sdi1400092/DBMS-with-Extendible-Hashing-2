#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "bf.h"
#include "hash_file.h"

#define RECORDS_NUM 40// you can change it if you want
#define GLOBAL_DEPT 2 // you can change it if you want
#define FILE_NAME "data.db"
#define FILE_NAME2 "data2.db"

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
  
  CALL_OR_DIE(HT_Init());
  CALL_OR_DIE(SHT_Init());

  int indexDesc,indexDesc_for_2nd_dir, indexDesc2, indexDesc2_for_2nd_dir;

  CALL_OR_DIE(HT_CreateIndex(FILE_NAME, GLOBAL_DEPT));
  CALL_OR_DIE(HT_OpenIndex(FILE_NAME, &indexDesc)); 
  CALL_OR_DIE(SHT_CreateSecondaryIndex("dir","sunames",20,GLOBAL_DEPT,FILE_NAME));
  CALL_OR_DIE(SHT_OpenSecondaryIndex("dir",&indexDesc_for_2nd_dir));

  CALL_OR_DIE(HT_CreateIndex(FILE_NAME2, GLOBAL_DEPT));
  CALL_OR_DIE(HT_OpenIndex(FILE_NAME2, &indexDesc2)); 
  CALL_OR_DIE(SHT_CreateSecondaryIndex("dir2","surnames",20,GLOBAL_DEPT,FILE_NAME2));
  CALL_OR_DIE(SHT_OpenSecondaryIndex("dir2",&indexDesc2_for_2nd_dir));

  Record record;
  SecondaryRecord secondary_record;
  UpdateRecordArray *update_record_array, *update_record_array2;
  srand(12569874);
  int r,tuppleid;
  // printf("Insert Entries\n");
  // for (int id = 0; id < RECORDS_NUM; ++id) {
  //   update_record_array = NULL;
  //   // create a record
  //   record.id = id;
  //   r = rand() % 12;
  //   memcpy(record.name, names[r], strlen(names[r]) + 1);
  //   r = rand() % 12;
  //   memcpy(record.surname, surnames[r], strlen(surnames[r]) + 1);
  //   r = rand() % 10;
  //   memcpy(record.city, cities[r], strlen(cities[r]) + 1);

  //   CALL_OR_DIE(HT_InsertEntry(indexDesc, record, &tuppleid, &update_record_array));
  //   strcpy(secondary_record.index_key,record.surname);
  //   secondary_record.tupleId=tuppleid;
  //   CALL_OR_DIE(SHT_SecondaryInsertEntry(indexDesc_for_2nd_dir,secondary_record));
  //   CALL_OR_DIE(SHT_SecondaryUpdateEntry(indexDesc_for_2nd_dir,update_record_array));
  // }

  srand(12345);
  printf("Insert Entries 2\n");
  for (int id = 0; id < RECORDS_NUM; ++id) {
    update_record_array2 = NULL;
    // create a record
    record.id = id;
    r = rand() % 12;
    memcpy(record.name, names[r], strlen(names[r]) + 1);
    r = rand() % 12;
    memcpy(record.surname, surnames[r], strlen(surnames[r]) + 1);
    r = rand() % 10;
    memcpy(record.city, cities[r], strlen(cities[r]) + 1);

    tuppleid = 0;
    CALL_OR_DIE(HT_InsertEntry(indexDesc2, record, &tuppleid, &update_record_array2));
    strcpy(secondary_record.index_key,record.surname);
    secondary_record.tupleId=tuppleid;
    CALL_OR_DIE(SHT_SecondaryInsertEntry(indexDesc2_for_2nd_dir,secondary_record));
    CALL_OR_DIE(SHT_SecondaryUpdateEntry(indexDesc2_for_2nd_dir,update_record_array2));
  }

  // char name[20];
  // r = rand() % 12;
  // strcpy(name, surnames[r]);
  // printf("Finding records with name %s\n", "Oikonomou");
  // CALL_OR_DIE(SHT_PrintAllEntries(indexDesc_for_2nd_dir, "Oikonomou"));
  // SHT_CloseSecondaryIndex(indexDesc_for_2nd_dir);
  // SHT_HashStatistics("dir");

  // printf("calling inner join\n");
  // SHT_InnerJoin(indexDesc_for_2nd_dir, indexDesc2_for_2nd_dir, NULL);

  // printf("RUN PrintAllEntries\n");
  // int id = 15;
  // CALL_OR_DIE(HT_PrintAllEntries(indexDesc, &id));
  // CALL_OR_DIE(HT_PrintAllEntries(indexDesc, NULL));
  // printf("\n\n\n");
  // CALL_OR_DIE(HT_PrintAllEntries(indexDesc2, NULL));


  // CALL_OR_DIE(HT_CloseFile(indexDesc));
  // BF_Close();
}
