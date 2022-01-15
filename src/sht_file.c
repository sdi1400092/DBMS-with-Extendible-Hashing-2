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
int Power(int base, int exp){
  int result = 1;
  while(exp!=0){
    result *= base;
    --exp;
  }
  return result;
}

void PrintRecord(Record record){
  printf("Id: %d\n",record.id);
  int i;
  printf("Name: ");
  printf("%s",record.name);
  printf("\n");
  printf("Surname: ");
  printf("%s",record.surname);
  printf("\n");
  printf("City: ");
  printf("%s",record.city);
  printf("\n");
}

void SHT_InnerJoin_print(SecondaryRecord a, SecondaryRecord b, int indesdesc1, int indesdesc2){
  int blockID1, blockID2, index_in_block1, index_in_block2;
  char *data1,*data2;

  Record *record1, *record2;
  record1 = (Record *) malloc(sizeof(Record));
  record2 = (Record *) malloc(sizeof(Record));

  BF_Block *block1, *block2;
  BF_Block_Init(&block1);
  BF_Block_Init(&block2);

  blockID1= (a.tupleId/8) -1;
  index_in_block1= a.tupleId%8;

  blockID2= (b.tupleId/8) -1;
  index_in_block2= b.tupleId%8;

  BF_GetBlock(indesdesc1, blockID1, block1);
  data1= BF_Block_GetData(block1);

  BF_GetBlock(indesdesc2, blockID2, block2);
  data2= BF_Block_GetData(block2);

  memcpy(record1, data1+(index_in_block1*sizeof(Record)), sizeof(Record));
  memcpy(record2, data2+(index_in_block2*sizeof(Record)), sizeof(Record));

  printf("%s %d %s %s ", record1->surname, record1->id, record1->name, record1->city);
  printf("%s %d %s %s\n", record2->surname, record2->id, record2->name, record2->city);

  BF_UnpinBlock(block1);
  BF_UnpinBlock(block2);
  free(record1);
  free(record2);

}

typedef struct{
  char primary_index_name[20];
  int indexdesc;
}Info;

Info Open_files[MAX_OPEN_FILES];

void HashFunction_SHT(char *key, int depth, int **hashing){
  int counter = 0, i = 0, *binary;
  while(key[i] != '\0'){
    counter += key[i];
    i++;
  }
  
  binary = (int *) malloc(depth*sizeof(int));
  for(i=0;i<depth;i++){
    binary[i] = counter%2;
    counter = counter/2;
  }
  *hashing = binary;
}

void HashFunction_bucket(int id, int depth, int **hashing){
  int i, *binary;
  binary = (int *) malloc(depth*sizeof(int));
  for(i=0;i<depth;i++){
    binary[i] = id%2;
    id = id/2;
  }
  *hashing = binary;
}

//a function that updates the existing directory in the blocks and allocates any new blocks that are needed
//in case the existing ones area not enough for storing the whole directory
HT_ErrorCode updateDirectory_SHT(Secondary_Directory *SHT, int indexdesc){

  char *data, *data2;
  BF_Block *tempBlock, *tempBlock2;
  BF_Block_Init(&tempBlock);
  BF_Block_Init(&tempBlock2);

  CALL_BF(BF_GetBlock(indexdesc,0,tempBlock));
  data=BF_Block_GetData(tempBlock);
  memcpy(data, &(SHT->global_depth), sizeof(int));
  memcpy(data+sizeof(int), &(SHT->max_buckets), sizeof(int));

  if(Power(2,SHT->global_depth)<=SHT->max_buckets){
    int i, num_of_buckets=Power(2,SHT->global_depth), num_of_next_block=-1;
    memcpy(data+2*sizeof(int), &(num_of_buckets), sizeof(int));
    memcpy(data+3*sizeof(int), &(num_of_next_block), sizeof(int));
    memcpy(data+4*sizeof(int), SHT->name_of_primary_index, sizeof(SHT->name_of_primary_index));
    for( i=0;i<Power(2,SHT->global_depth);i++){
      memcpy(data+4*sizeof(int)+sizeof(SHT->name_of_primary_index)+(i*sizeof(buckets)),&(SHT->bucket[i]),sizeof(buckets));
    }
    BF_Block_SetDirty(tempBlock);
    CALL_BF(BF_UnpinBlock(tempBlock));
  }
  else{
    int i, counter = Power(2, SHT->global_depth);
    int offset = 0;
    for( i=0;i<SHT->max_buckets;i++){
      memcpy(data+4*sizeof(int)+sizeof(SHT->name_of_primary_index)+(i*sizeof(buckets)),&(SHT->bucket[offset]),sizeof(buckets));
      counter--;
      offset++;
    }
    memcpy(data+2*sizeof(int), &i, sizeof(int)); //update number of buckets 
    int x= -1, temp_numofnextblock;
    memcpy(&x,data+3*sizeof(int),sizeof(int)); //x=number of next block
    temp_numofnextblock = 0;
    BF_Block_SetDirty(tempBlock);
    CALL_BF(BF_UnpinBlock(tempBlock));

    while(x>0){
      CALL_BF(BF_GetBlock(indexdesc, x, tempBlock2));
      data2 = BF_Block_GetData(tempBlock2);
      for( i=0;i<SHT->max_buckets && counter>0 ; i++){
        memcpy(data2+4*sizeof(int)+sizeof(SHT->name_of_primary_index)+(i*sizeof(buckets)),&(SHT->bucket[offset]),sizeof(buckets));
        counter--;
        offset++;
      }
      memcpy(data2+2*sizeof(int), &i, sizeof(int)); //update number of buckets 
      temp_numofnextblock = x;
      memcpy(&x, data2+3*sizeof(int), sizeof(int));
      BF_Block_SetDirty(tempBlock2);
      CALL_BF(BF_UnpinBlock(tempBlock2));
    }

    if(counter>0){
      int temp=counter;
      do{
        //allocate new blocks for directory
        CALL_BF(BF_AllocateBlock(indexdesc, tempBlock2));
        data2 = BF_Block_GetData(tempBlock2);

        //set number of next block for the last allocated block
        CALL_BF(BF_GetBlock(indexdesc, temp_numofnextblock, tempBlock));
        data = BF_Block_GetData(tempBlock);
        int block_num;
        CALL_BF(BF_GetBlockCounter(indexdesc, &block_num));
        block_num--;
        memcpy(data+3*sizeof(int), &(block_num), sizeof(int));
        BF_Block_SetDirty(tempBlock);
        CALL_BF(BF_UnpinBlock(tempBlock));

        memcpy(data2+3*sizeof(int), &x, sizeof(int)); //x=number of next block
        int j=0;
        while(counter>0 && j<SHT->max_buckets){
          memcpy(data2+4*sizeof(int)+sizeof(SHT->name_of_primary_index)+(j*sizeof(buckets)), &(SHT->bucket[offset]), sizeof(buckets));
          counter--;
          offset++;
          j++;
        }
        memcpy(data2+2*sizeof(int), &j, sizeof(int)); //update number of buckets 
        temp_numofnextblock = block_num;
        BF_Block_SetDirty(tempBlock2);
        CALL_BF(BF_UnpinBlock(tempBlock2));
      }while(counter>0);
    }
  }
  return HT_OK;
}

//a function that copies the existing directory from the blocks of the file to an existing variable HT 
//passed by reference
void getDirectory_SHT(Secondary_Directory **SHT, int indexdesc){  
  int *hashing, i, counter=0, number_of_buckets, next_block;
  char *data, *data2;
  BF_Block *Dirblock;
  BF_Block_Init(&Dirblock);

  BF_GetBlock(indexdesc, 0, Dirblock);
  data = BF_Block_GetData(Dirblock);

  memcpy(&((*SHT)->global_depth), data, sizeof(int));
  memcpy(&((*SHT)->max_buckets), data+sizeof(int), sizeof(int));
  memcpy(&(number_of_buckets), data+2*sizeof(int), sizeof(int));
  memcpy(&((*SHT)->name_of_primary_index), data+4*sizeof(int), sizeof((*SHT)->name_of_primary_index));
  (*SHT)->bucket = (buckets *) malloc(Power(2,(*SHT)->global_depth) * sizeof(buckets));
  int a=0;
  
  while(counter<Power(2, (*SHT)->global_depth)){
    for(int j=0 ; j<number_of_buckets ; j++){
      memcpy(&((*SHT)->bucket[counter]), data+4*sizeof(int)+sizeof((*SHT)->name_of_primary_index)+j*sizeof(buckets), sizeof(buckets));
      HashFunction_bucket(counter,(*SHT)->global_depth,&((*SHT)->bucket[counter].HashCode));
      counter++;
      }
    memcpy(&(next_block), data+3*sizeof(int), sizeof(int));
    BF_UnpinBlock(Dirblock);
    if(next_block>0){
      BF_GetBlock(indexdesc, next_block, Dirblock);
      data = BF_Block_GetData(Dirblock);
      memcpy(&(number_of_buckets), data+2*sizeof(int), sizeof(int));
    }
  }
}

HT_ErrorCode SHT_Init() {
  //insert code here
  int i;
  for(i=0;i<MAX_OPEN_FILES;i++){
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
  SHT->bucket = (buckets *) malloc(Power(2,depth)*sizeof(buckets));
  for(i=0;i<Power(2,depth);i++){
    //gives starting values to every bucket
    SHT->bucket[i].local_depth = depth;
    SHT->bucket[i].maxSize = MAX_SIZE_OF_BUCKET/(sizeof(SecondaryRecord));
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
  for(i=0;i<Power(2,depth);i++){ 
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
  x = Power(2, SHT->global_depth);
  memcpy(data+2*sizeof(int), &x, sizeof(int)); //x is the number of how many buckets are stored in the block
  x = -1;
  memcpy(data+3*sizeof(int), &x, sizeof(int)); //here x is the number of the next block if its -1 it means there's no next block
  memcpy(data+4*sizeof(int), &SHT->name_of_primary_index, sizeof(SHT->name_of_primary_index));
  for( i=0;i<Power(2,SHT->global_depth);i++){
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
  char *data;
  data= BF_Block_GetData(temp_block);
  char fileName[20];
  memcpy(&fileName,data+4*(sizeof(int)),20*sizeof(char));
  for(i=0 ; i < MAX_OPEN_FILES ; i++){
    if(Open_files[i].indexdesc == -1){
      strcpy(Open_files[i].primary_index_name,fileName);
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
      Open_files[i].indexdesc =-1;
      break;
    }
  }
  CALL_BF(BF_CloseFile(indexDesc));
  return HT_OK;
}

HT_ErrorCode SHT_SecondaryInsertEntry (int indexDesc, SecondaryRecord record  ) {
  //insert code here
  int *hashing, i, offset;
  char *data, *data2;
  SecondaryRecord *temp = &record;
  Secondary_Directory *SHT;
  SHT = (Secondary_Directory *) malloc(sizeof(Secondary_Directory));
  BF_Block *block;
  BF_Block_Init(&block);

  getDirectory_SHT(&SHT, indexDesc);

  //Hash Function call here for record
  HashFunction_SHT(record.index_key, SHT->global_depth, &hashing);

  //find in which bucket the record we want to insert should go
  int counter;
  for(i=0;i<(Power(2,SHT->global_depth));i++){
    counter=0;
    for(int j=0;j<(SHT->bucket[i].local_depth);j++){
      if (SHT->bucket[i].HashCode[j]== hashing[j]){
        counter++;
      }
    }
    if(counter == SHT->bucket[i].local_depth){
      break;
    }
  }

  //we get that block from the memory
  CALL_BF(BF_GetBlock(indexDesc, SHT->bucket[i].number_of_block, block));
  data2 = BF_Block_GetData(block);
  offset = SHT->bucket[i].number_of_registries;
  //if the block has space for the registry we store it in there 
  if (offset < SHT->bucket[i].maxSize){
    memcpy(data2 + offset*sizeof(SecondaryRecord), temp, sizeof(SecondaryRecord));
    BF_Block_SetDirty(block);
    CALL_BF(BF_UnpinBlock(block));
    SHT->bucket[i].number_of_registries++;
    if(SHT->bucket[i].local_depth<SHT->global_depth){
      for(int a=0 ; a<Power(2, SHT->global_depth) ; a++){
        if(SHT->bucket[i].number_of_block == SHT->bucket[a].number_of_block && a!=i){
          SHT->bucket[a].number_of_registries++;
        }
      }
    }
    //after updating the directory we store the updated one in the memory
    updateDirectory_SHT(SHT, indexDesc);

    getDirectory_SHT(&SHT, indexDesc);

    printf("\n\n");
    for(int a=0 ; a<Power(2, SHT->global_depth) ; a++){
      BF_GetBlock(indexDesc, SHT->bucket[a].number_of_block, block);
      data = BF_Block_GetData(block);
      printf("block %d with %d records:\n", SHT->bucket[a].number_of_block, SHT->bucket[a].number_of_registries);
      for(int b=0 ; b<SHT->bucket[a].number_of_registries ; b++){
        memcpy(temp, data+(b*sizeof(SecondaryRecord)), sizeof(SecondaryRecord));
        printf("%s %d\n", temp->index_key, temp->tupleId);
      }
    }
    printf("\n\n");

  }
  //if there's no space in that bucket we have two cases
  else{
    //if the local depth of the bucket is equal to the global depth it means that we need to expand
    if(SHT->bucket[i].local_depth == SHT->global_depth){
      //expand
      SHT->global_depth += 1;
      SHT->bucket=(buckets *) realloc(SHT->bucket,Power(2,SHT->global_depth)*sizeof(buckets));
      int z=0;
      for(int j=(Power(2,SHT->global_depth-1));j<(Power(2,SHT->global_depth));j+=1){
        SHT->bucket[j]=SHT->bucket[z];
        z+=1;
      }
      //Hashing
      for(int j=0;j<Power(2,SHT->global_depth);j++){
        SHT->bucket[j].HashCode = (int *) malloc(SHT->global_depth*sizeof(int));
        HashFunction_bucket(j, SHT->global_depth, &(SHT->bucket[j].HashCode));
      }
    }
    //either if the expand took place or not we can split the existing bucket into two
    //since the expand only creates the extra indexes for the buckets and doesnt create the actuall
    //space for them
    //split
    BF_Block *temp_block;
    BF_Block_Init(&temp_block);
    //allocate a new block for the split and change the values of the buckets that are being seperated
    CALL_BF(BF_AllocateBlock(indexDesc, temp_block));
    int k=(SHT->global_depth)-(SHT->bucket[i].local_depth);
    int z=i;
    for(int j=1;j<=Power(2,k);j++){
      SHT->bucket[z].local_depth+=1;
      z += j*Power(2, SHT->bucket[i].local_depth-1);
    }
    int blocknum;
    BF_GetBlockCounter(indexDesc, &blocknum);
    z=i;
    for(int j=1;j<=(Power(2,k)/2);j++){
      z += j*Power(2, SHT->bucket[i].local_depth-1);
      SHT->bucket[z].number_of_block = blocknum - 1;
      SHT->bucket[z].number_of_registries = 0;
    }

    BF_Block_SetDirty(temp_block);
    CALL_BF(BF_UnpinBlock(temp_block));

    int x = SHT->bucket[i].number_of_registries;
    SHT->bucket[i].number_of_registries = 0;


    updateDirectory_SHT(SHT, indexDesc);

    //call the insert recursively to reorganize the existing registries of the bucket that got splited
    SecondaryRecord *temp_record;
    temp_record = (SecondaryRecord *) malloc(sizeof(SecondaryRecord));
    for(int j=0 ; j<x ; j++){
      memcpy(temp_record, data2 + j*sizeof(SecondaryRecord), sizeof(SecondaryRecord));
      SHT_SecondaryInsertEntry(indexDesc, *temp_record);
    }
    free(temp_record);
    //call insert one last time for the original registry
    SHT_SecondaryInsertEntry(indexDesc, record);
  }

  // getDirectory_SHT(&SHT, indexDesc);

  // printf("\n\n");
  // for(int a=0 ; a<Power(2, SHT->global_depth) ; a++){
  //   BF_GetBlock(indexDesc, SHT->bucket[a].number_of_block, block);
  //   data = BF_Block_GetData(block);
  //   printf("block %d with %d records:\n", SHT->bucket[a].number_of_block, SHT->bucket[a].number_of_registries);
  //   for(int b=0 ; b<SHT->bucket[a].number_of_registries ; b++){
  //     memcpy(temp, data+(b*sizeof(SecondaryRecord)), sizeof(SecondaryRecord));
  //     printf("%s %d\n", temp->index_key, temp->tupleId);
  //   }
  // }
  // printf("\n\n");

  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));

  free(SHT->bucket);
  free(SHT);

  return HT_OK;
}

HT_ErrorCode SHT_SecondaryUpdateEntry (int indexDesc, UpdateRecordArray *updateArray ) {
  //insert code here
  int *hashing, i;
  char *data, *data2;
  SecondaryRecord temp, temp2;
  Secondary_Directory *SHT;
  SHT = (Secondary_Directory *) malloc(sizeof(Secondary_Directory));
  BF_Block *block;
  BF_Block_Init(&block);

  getDirectory_SHT(&SHT, indexDesc);
  int z = 0;
  if(updateArray!=NULL){
    while(updateArray[z].surname[0]!= '\0'){
      HashFunction_SHT(updateArray[z].surname, SHT->global_depth, &hashing);

      //find in which bucket the record we want to change its tupleID is
      int counter;
      for(i=0;i<(Power(2,SHT->global_depth));i++){
        counter=0;
        for(int j=0;j<(SHT->bucket[i].local_depth);j++){
          if (SHT->bucket[i].HashCode[j]== hashing[j]){
            counter++;
          }
        }
        if(counter == SHT->bucket[i].local_depth){
          break;
        }
      }
      
      for(int j=0 ; j<SHT->bucket[i].number_of_registries ; j++){
        CALL_BF(BF_GetBlock(indexDesc, SHT->bucket[i].number_of_block, block));
        data2 = BF_Block_GetData(block);
        memcpy(&temp, data2+(j*sizeof(SecondaryRecord)), sizeof(SecondaryRecord));
        if((strcmp(temp.index_key, updateArray[z].surname) == 0) && (temp.tupleId = updateArray[z].oldTupleId)){
          temp.tupleId = updateArray[z].newTupleId;
          memcpy(data2+(j*sizeof(SecondaryRecord)), &temp, sizeof(SecondaryRecord));
          BF_Block_SetDirty(block);
          CALL_BF(BF_UnpinBlock(block));
        }
      }
      z++;
    }
  }

  free(SHT->bucket);
  free(SHT);

  return HT_OK;
}

HT_ErrorCode SHT_PrintAllEntries(int sindexDesc, char *index_key ) {
  //insert code here
  int *hashing, i, blockID, index_in_block, indexDesc, printed = 0;
  char *data, *data2;
  Secondary_Directory *SHT;
  SHT = (Secondary_Directory *)malloc(sizeof(Secondary_Directory));
  getDirectory_SHT(&SHT, sindexDesc);

  HashFunction_SHT(index_key, SHT->global_depth, &hashing);
  int counter, block_num;
  for(i=0 ; i<(Power(2,SHT->global_depth)) ; i++){
    counter=0;
    for(int j=(SHT->bucket[i].local_depth)-1;j>=0;j--){
      if (SHT->bucket[i].HashCode[j]== hashing[j]){
        counter++;
      }
    }
    if(counter == SHT->bucket[i].local_depth){
      break;
    }
  }

  char * filename;
  BF_Block *block, *block2;
  BF_Block_Init(&block);
  BF_Block_Init(&block2);
  SecondaryRecord *temp;
  temp = (SecondaryRecord *) malloc(sizeof(SecondaryRecord));
  Record *record;
  record = (Record *)malloc(sizeof(Record));
  CALL_BF(BF_GetBlock(sindexDesc, SHT->bucket[i].number_of_block, block));
  data = BF_Block_GetData(block);

  for(int z=0 ; z<MAX_OPEN_FILES ; z++){
    if(sindexDesc == Open_files[z].indexdesc){
      filename = Open_files[z].primary_index_name;
      break;
    }
  }


  CALL_BF(BF_OpenFile(SHT->name_of_primary_index, &indexDesc));

  for(int j=0 ; j<SHT->bucket[i].number_of_registries ; j++){

    memcpy(temp, data + (j*sizeof(SecondaryRecord)), sizeof(SecondaryRecord));

    //calculate blockID and index_in_block here...
    blockID = (temp->tupleId / 8) - 1;
    index_in_block = temp->tupleId % 8;

    if(strcmp(temp->index_key, index_key) == 0){
      BF_GetBlock(indexDesc, blockID, block2);
      data2 = BF_Block_GetData(block2);
      memcpy(record, data2+(index_in_block*sizeof(Record)), sizeof(Record));
      PrintRecord(*record);
      printed++;
      BF_UnpinBlock(block2);
    }
  }

  printf("\n\n");
  for(int a=0 ; a<Power(2, SHT->global_depth) ; a++){
    BF_GetBlock(sindexDesc, SHT->bucket[a].number_of_block, block2);
    data = BF_Block_GetData(block2);
    printf("block %d with %d records:\n", SHT->bucket[a].number_of_block, SHT->bucket[a].number_of_registries);
    for(int b=0 ; b<SHT->bucket[a].number_of_registries ; b++){
      memcpy(temp, data+(b*sizeof(SecondaryRecord)), sizeof(SecondaryRecord));
      printf("%s %d\n", temp->index_key, temp->tupleId);
    }
  }
  printf("\n\n");

  //CALL_BF(BF_CloseFile(indexDesc));
  
  BF_UnpinBlock(block);

  free(record);
  free(temp);
  free(SHT->bucket);
  free(SHT);

  if(printed == 0){
    printf("No registry found with surname = %s please try again\n", index_key);
  }
  else{
    printf("Records found: %d\n", printed);
  }

  return HT_OK;
}

HT_ErrorCode SHT_HashStatistics(char *filename ) {
  //insert code here
  int totalBlocks, sindexDesc, blocknum;
  char *data;
  SecondaryRecord *record;
  record = (SecondaryRecord *) malloc(sizeof(SecondaryRecord));
  BF_Block *block;
  BF_Block_Init(&block);
  Secondary_Directory *SHT;
  SHT = (Secondary_Directory *) malloc(sizeof(Secondary_Directory));

  CALL_BF(BF_OpenFile(filename, &sindexDesc));

  CALL_BF(BF_GetBlockCounter(sindexDesc, &blocknum));

  printf("file %s has a total of %d blocks\n", filename, blocknum);

  int min=100, max=0;
  float sum=0;

  getDirectory_SHT(&SHT, sindexDesc);

  for(int i=0 ; i<Power(2, SHT->global_depth) ; i++){
    if(SHT->bucket[i].number_of_registries < min) min = SHT->bucket[i].number_of_registries;
    if(SHT->bucket[i].number_of_registries > max) max = SHT->bucket[i].number_of_registries;
    sum = sum + SHT->bucket[i].number_of_registries;
  }
  float avg;
  avg = (float) (sum/(float) Power(2, SHT->global_depth));
  printf("Minimum registries in a bucket are: %d, Maximum are: %d and average are: %f\n", min, max, avg);

  return HT_OK;
}

HT_ErrorCode SHT_InnerJoin(int sindexDesc1, int sindexDesc2,  char *index_key ) {
  //insert code here
  int *hashing, *hashing2,indexdesc_primary_index1,indexdesc_primary_index2;
  char *data1, *data2;
  BF_Block *block1, *block2;
  BF_Block_Init(&block1);
  BF_Block_Init(&block2);

  Secondary_Directory *SHT1, *SHT2;
  SHT1 = (Secondary_Directory *) malloc(sizeof(Secondary_Directory));
  SHT2 = (Secondary_Directory *) malloc(sizeof(Secondary_Directory));

  getDirectory_SHT(&SHT1, sindexDesc1);
  getDirectory_SHT(&SHT2, sindexDesc2);

  CALL_BF(BF_OpenFile(SHT1->name_of_primary_index, &indexdesc_primary_index1));
  CALL_BF(BF_OpenFile(SHT2->name_of_primary_index, &indexdesc_primary_index2));

  if (index_key != NULL){

    HashFunction_SHT(index_key, SHT1->global_depth, &hashing);

    int counter, i;
    for(i=0 ; i<(Power(2,SHT1->global_depth)) ; i++){
      counter=0;
      for(int j=(SHT1->bucket[i].local_depth)-1;j>=0;j--){
        if (SHT1->bucket[i].HashCode[j]== hashing[j]){
          counter++;
        }
      }
      if(counter == SHT1->bucket[i].local_depth){
        break;
      }
    }
  
    int j;
    for(j=0 ; j<(Power(2,SHT2->global_depth)) ; j++){
      counter=0;
      for(int z=(SHT2->bucket[i].local_depth)-1;z>=0;z--){
        if (SHT2->bucket[j].HashCode[z]== hashing[z]){
          counter++;
        }
      }
      if(counter == SHT2->bucket[j].local_depth){
        break;
      }
    }

    CALL_BF(BF_GetBlock(sindexDesc1, SHT1->bucket[i].number_of_block, block1));
    CALL_BF(BF_GetBlock(sindexDesc2, SHT2->bucket[j].number_of_block, block2));

    data1 = BF_Block_GetData(block1);
    data2 = BF_Block_GetData(block2);

    SecondaryRecord temp1, temp2;

    for(int a=0 ; a<SHT1->bucket[i].number_of_registries ; a++){
      memcpy(&temp1, data1+(a*sizeof(SecondaryRecord)), sizeof(SecondaryRecord));
      if(strcmp(temp1.index_key, index_key) == 0){
        for(int b=0 ; b<SHT2->bucket[j].number_of_registries ; b++){
          memcpy(&temp2, data2+(b*sizeof(SecondaryRecord)), sizeof(SecondaryRecord));
          if(strcmp(temp2.index_key, index_key) == 0){
            printf("\nSHT1: %s, %d\n", temp1.index_key, temp1.tupleId);
            printf("SHT2: %s, %d\n", temp2.index_key, temp2.tupleId);
            SHT_InnerJoin_print(temp1, temp2, indexdesc_primary_index1, indexdesc_primary_index2);
          }
        }
      }
    }

    CALL_BF(BF_UnpinBlock(block1));
    CALL_BF(BF_UnpinBlock(block2));

    free(SHT1->bucket);
    free(SHT1);
    free(SHT2->bucket);
    free(SHT2);

  }
  else{

    int counter;
    for(int i=0 ; i<Power(2, SHT1->global_depth) ; i++){
      HashFunction_bucket(SHT1->bucket[i].number_of_block, SHT1->global_depth, &hashing);
      for(int j=0 ; j<Power(2, SHT2->global_depth) ; j++){
        HashFunction_bucket(SHT2->bucket[j].number_of_block, SHT2->global_depth, &hashing2);
        counter = 0;
        int min= SHT1->global_depth;
        if(SHT1->global_depth>SHT2->global_depth){
          min=SHT2->global_depth;
        }
        for(int a=0 ; a<min ; a++){
          if(hashing[a] == hashing2[a]){
            counter++;
          }
        }
        if(counter == min){
          CALL_BF(BF_GetBlock(sindexDesc1, SHT1->bucket[i].number_of_block, block1));
          CALL_BF(BF_GetBlock(sindexDesc2, SHT2->bucket[j].number_of_block, block2));

          data1 = BF_Block_GetData(block1);
          data2 = BF_Block_GetData(block2);

          SecondaryRecord temp1, temp2;

          for(int q=0 ; q<SHT1->bucket[i].number_of_registries ; q++){
            memcpy(&temp1, data1+(q*sizeof(SecondaryRecord)), sizeof(SecondaryRecord));
            //printf("\nSHT1: %s, %d\n", temp1.index_key, temp1.tupleId);
            for(int w=0 ; w<SHT2->bucket[j].number_of_registries ; w++){
              memcpy(&temp2, data2+(w*sizeof(SecondaryRecord)), sizeof(SecondaryRecord));
              //printf("SHT2: %s, %d\n", temp2.index_key, temp2.tupleId);
              if(strcmp(temp2.index_key, temp1.index_key) == 0){
                //printf("1: %s %d\n2: %s %d\n", temp1.index_key, temp1.tupleId, temp2.index_key, temp2.tupleId);
                SHT_InnerJoin_print(temp1, temp2, indexdesc_primary_index1, indexdesc_primary_index2);
              }
            }
          }
        }

      }
    }
  }

  return HT_OK;
}


// #endif // HASH_FILE_H