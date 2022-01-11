#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "sht_file.h"
#define MAX_OPEN_FILES 20

#define MAX_SIZE_OF_BUCKET BF_BLOCK_SIZE

// #ifndef SHT_FILE_H
// #define SHT_FILE_H

#define CALL_BF(call)       \
{                           \
  BF_ErrorCode code = call; \
  if (code != BF_OK) {         \
    BF_PrintError(code);    \
    return HT_ERROR;        \
  }                         \
}

// typedef enum HT_ErrorCode {
//   HT_OK,
//   HT_ERROR
// } HT_ErrorCode;

// typedef struct Record {
// 	int id;
// 	char name[15];
// 	char surname[20];
// 	char city[20];
// } Record;

// typedef struct{
// char index_key[20];
// int tupleId;  /*Ακέραιος που προσδιορίζει το block και τη θέση μέσα στο block στην οποία     έγινε η εισαγωγή της εγγραφής στο πρωτεύον ευρετήριο.*/ 
// }SecondaryRecord;


typedef struct {
  int *HashCode;
  int number_of_block; //με τη παραδοχη οτι 1 block = 1 καδος
  int number_of_registries;
  int local_depth;
  int maxSize;
}buckets;

typedef struct {
  buckets *bucket;
  int global_depth;
  int max_buckets;
  char name_of_primary_index[20];
}Secondary_Directory;

//power calculates the x^y expression 
int power(int base, int exp){
  int result = 1;
  while(exp!=0){
    result *= base;
    --exp;
  }
  return result;
}

typedef struct{
  char *primary_index_name;
  int indexdesc;
}Info;

Info Open_files[MAX_OPEN_FILES];

HT_ErrorCode SHT_Init() {
  //insert code here
  int i;
  for(i=0;i<MAX_OPEN_FILES;i++){
    Open_files[i].primary_index_name=NULL;
    Open_files[i].indexdesc =-1;  
  }
  return HT_OK;
}

HT_ErrorCode SHT_CreateSecondaryIndex(const char *sfileName, char *attrName, int attrLength, int depth,char *fileName ) {
  //insert code here
    //creating HT and allocating required space for it
  int id, i, j, n, file_desc, **binary;
  char *data;
  Secondary_Directory *SHT;
  SHT = (Secondary_Directory *) malloc(sizeof(Secondary_Directory));
  //allocating space for 2^depth buckets
  SHT->bucket = (buckets *) malloc(power(2,depth)*sizeof(buckets));
  for(i=0;i<power(2,depth);i++){
    //gives starting values to every bucket
    SHT->bucket[i].local_depth = depth;
    SHT->bucket[i].maxSize = MAX_SIZE_OF_BUCKET/(sizeof(struct Record));
    SHT->bucket[i].number_of_registries = 0;
    n=i;
    SHT->bucket[i].HashCode =malloc(depth*sizeof(int));
    for(j=0;j<depth;j++){
      SHT->bucket[i].HashCode[j]=n%2;
      n=n/2;
    }
  }
  strcpy(SHT->name_of_primary_index,fileName);
  //giving starting values to HashTable
  SHT->global_depth = depth;
  SHT->max_buckets= (BF_BLOCK_SIZE-((4*sizeof(int))-sizeof(SHT->name_of_primary_index)))/sizeof(buckets);

  //opens file and allocates one block for the directory
  CALL_BF(BF_OpenFile(sfileName, &file_desc));
  BF_Block *temp_block;
  BF_Block_Init(&temp_block);
  CALL_BF(BF_AllocateBlock(file_desc, temp_block));
  int block_num;
  data = BF_Block_GetData(temp_block);

  //allocates one block for every bucket
  BF_Block *temp_block2;
  BF_Block_Init(&temp_block2);
  for(i=0;i<power(2,depth);i++){ 
    CALL_BF(BF_AllocateBlock(file_desc, temp_block2));
    CALL_BF(BF_GetBlockCounter(file_desc,&block_num));
    SHT->bucket[i].number_of_block= block_num -1;
    //setting the blocks for the buckets dirty and upnin them
    BF_Block_SetDirty(temp_block2);
    CALL_BF(BF_UnpinBlock(temp_block2));
  }
      //copying the directory to the first block of the file
  memcpy(data, &(SHT->global_depth), sizeof(int));
  memcpy(data+sizeof(int), &(SHT->max_buckets), sizeof(int));
  int x;
  x = power(2, SHT->global_depth);
  memcpy(data+2*sizeof(int), &x, sizeof(int)); //x is the number of how many buckets are stored in the block
  x = -1;
  memcpy(data+3*sizeof(int), &x, sizeof(int)); //here x is the number of the next block if its -1 it means there's no next block
  memcpy(data+4*sizeof(int), &SHT->name_of_primary_index, sizeof(SHT->name_of_primary_index));
  for( i=0;i<power(2,SHT->global_depth);i++){
    memcpy(data+4*sizeof(int)+(sizeof(SHT->name_of_primary_index))+(i*sizeof(buckets)),&(SHT->bucket[i]),sizeof(buckets));
  }
  //setting dirty that block and upnining it
  BF_Block_SetDirty(temp_block);
  CALL_BF(BF_UnpinBlock(temp_block));

  printf("File: %s is created\n", sfileName);
  CALL_BF(BF_CloseFile(file_desc));
  free(SHT->bucket);
  free(SHT);
  return HT_OK;
}

HT_ErrorCode SHT_OpenSecondaryIndex(const char *sfileName, int *indexDesc  ) {
  //insert code here
  int i;
  int temp;
  CALL_BF(BF_OpenFile(sfileName,&temp));
  *indexDesc=temp;
  BF_Block *temp_block;
  BF_Block_Init(&temp_block);
  BF_GetBlock(*indexDesc,0,temp_block);
  char * data;
  data= BF_GET_DATA(temp_block);
  char fileName[20];
  memcpy(&fileName,data+4*(sizeof(int)),20*sizeof(char));
  for(i=0 ; i < MAX_OPEN_FILES ; i++){
    if(Open_files[i].primary_index_name == NULL){
      strcpy(Open_files[i].primary_index_name,fileName);
      break;
    }
  }
  for(i=0 ; i < MAX_OPEN_FILES ; i++){
    if(Open_files[i].indexdesc == -1){
      Open_files[i].indexdesc = *indexDesc;
      break;
    }
  }
  BF_UnpinBlock(temp_block);
  return HT_OK;
}

HT_ErrorCode SHT_CloseSecondaryIndex(int indexDesc) {
  //insert code here
  int i;
  for(i=0;i<MAX_OPEN_FILES;i++){
    if (Open_files[i].indexdesc =indexDesc){
      Open_files[i].primary_index_name=NULL;
      Open_files[i].indexdesc =-1;
      break;
    }
  }
  CALL_BF(BF_CloseFile(indexDesc));
  return HT_OK;
}

HT_ErrorCode SHT_SecondaryInsertEntry (int indexDesc,SecondaryRecord record  ) {
  //insert code here
  return HT_OK;
}

HT_ErrorCode SHT_SecondaryUpdateEntry (int indexDesc, UpdateRecordArray *updateArray ) {
  //insert code here
  return HT_OK;
}

HT_ErrorCode SHT_PrintAllEntries(int sindexDesc, char *index_key ) {
  //insert code here
  return HT_OK;
}

HT_ErrorCode SHT_HashStatistics(char *filename ) {
  //insert code here
  return HT_OK;
}

HT_ErrorCode SHT_InnerJoin(int sindexDesc1, int sindexDesc2,  char *index_key ) {
  //insert code here
  return HT_OK;
}


// #endif // HASH_FILE_H