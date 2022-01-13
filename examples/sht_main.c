#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "bf.h"
#include "hash_file.h"

#define RECORDS_NUM 85// you can change it if you want
#define GLOBAL_DEPT 2 // you can change it if you want
#define FILE_NAME "data.db"

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

  int indexDesc,indexDesc_for_2nd_dir;
  CALL_OR_DIE(HT_CreateIndex(FILE_NAME, GLOBAL_DEPT));
  CALL_OR_DIE(HT_OpenIndex(FILE_NAME, &indexDesc)); 
  CALL_OR_DIE(SHT_CreateSecondaryIndex("dir","sunames",20,GLOBAL_DEPT,FILE_NAME));
  CALL_OR_DIE(SHT_OpenSecondaryIndex("dir",&indexDesc_for_2nd_dir));

  Record record;
  SecondaryRecord secondary_record;
  UpdateRecordArray *update_record_array=NULL;
  srand(12569874);
  int r,tuppleid;
  printf("Insert Entries\n");
  for (int id = 0; id < RECORDS_NUM; ++id) {
    // create a record
    record.id = id;
    r = rand() % 12;
    memcpy(record.name, names[r], strlen(names[r]) + 1);
    r = rand() % 12;
    memcpy(record.surname, surnames[r], strlen(surnames[r]) + 1);
    r = rand() % 10;
    memcpy(record.city, cities[r], strlen(cities[r]) + 1);

    CALL_OR_DIE(HT_InsertEntry(indexDesc, record, &tuppleid, &update_record_array));
    strcpy(secondary_record.index_key,record.surname);
    secondary_record.tupleId=tuppleid;
    CALL_OR_DIE(SHT_SecondaryInsertEntry(indexDesc_for_2nd_dir,secondary_record));
    CALL_OR_DIE(SHT_SecondaryUpdateEntry(indexDesc_for_2nd_dir,update_record_array));
  }

  CALL_OR_DIE(SHT_PrintAllEntries(indexDesc_for_2nd_dir,"Halatsis�"));
  //SHT_CloseSecondaryIndex(indexDesc_for_2nd_dir);
  //SHT_HashStatistics("dir");

  // printf("RUN PrintAllEntries\n");
  // int id = rand() % RECORDS_NUM;
  // CALL_OR_DIE(HT_PrintAllEntries(indexDesc, &id));
  // CALL_OR_DIE(HT_PrintAllEntries(indexDesc, NULL));


  // CALL_OR_DIE(HT_CloseFile(indexDesc));
  // BF_Close();
}
