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
  for(i=0;i<15;i++){
    printf("%c",record.name[i]);
    //if(record.name[i+1]==NULL) break;
  }
  printf("\n");
  printf("Surname: ");
  for(i=0;i<20;i++){
    printf("%c",record.surname[i]);
    //if(record.surname[i+1]==NULL) break;
  }
  printf("\n");
  printf("City: ");
  for(i=0;i<20;i++){
    printf("%c",record.city[i]);
    //if(record.city[i+1]==NULL) break;
  }
  printf("\n");
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
  // for(i=0 ; i < MAX_OPEN_FILES ; i++){
  //   if(Open_files[i].indexdesc == -1){
  //     Open_files[i].indexdesc = *indexDesc;
  //     break;
  //   }
  // }
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
    //after updating the directory we store the updated one in the memory
    updateDirectory_SHT(SHT, indexDesc);

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
  SecondaryRecord temp;
  Secondary_Directory *SHT;
  SHT = (Secondary_Directory *) malloc(sizeof(Secondary_Directory));
  BF_Block *block;
  BF_Block_Init(&block);

  getDirectory_SHT(&SHT, indexDesc);
  int z = 0;
  if(updateArray!=NULL){
    while(updateArray[z].surname[0]!= '\0'){
      HashFunction_SHT(updateArray[z].surname, SHT->global_depth, &hashing);

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
      CALL_BF(BF_GetBlock(indexDesc, SHT->bucket[i].number_of_block, block));
      data2 = BF_Block_GetData(block);
      for(int j=0 ; j<SHT->bucket[i].number_of_registries ; j++){
        memcpy(&temp, data2+(j*sizeof(SecondaryRecord)), sizeof(SecondaryRecord));
        if((temp.index_key == updateArray[z].surname) && (temp.tupleId = updateArray[z].oldTupleId)){
          temp.tupleId = updateArray[z].newTupleId;
          BF_Block_SetDirty(block);
          break;
        }
      }
      CALL_BF(BF_UnpinBlock(block));
      z++;
    }
  }

  free(SHT->bucket);
  free(SHT);

  return HT_OK;
}

HT_ErrorCode SHT_PrintAllEntries(int sindexDesc, char *index_key ) {
  //insert code here
  int *hashing, i, blockID=1, index_in_block=0, indexDesc, printed = 0;
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

    if(temp->index_key == index_key){
      

      BF_GetBlock(indexDesc, blockID, block2);
      data2 = BF_Block_GetData(block2);
      memcpy(record, data2+(index_in_block*sizeof(Record)), sizeof(Record));
      PrintRecord(*record);
      printed++;
      BF_UnpinBlock(block2);
    }
  }

  CALL_BF(BF_CloseFile(indexDesc));
  
  BF_UnpinBlock(block);
  BF_Block *temporary;
  BF_Block_Init(&temporary);
  for(i=0 ; i<(Power(2,SHT->global_depth)) ; i++){
    printf("Number of block: %d, Number of registries: %d\n",SHT->bucket[i].number_of_block,SHT->bucket[i].number_of_registries);
  }
  for(i=0 ; i<(Power(2,SHT->global_depth)) ; i++){
    printf("I: %d\n",i);
    BF_GetBlock(sindexDesc, SHT->bucket[i].number_of_block , temporary);
    data2 = BF_Block_GetData(temporary);
    for(int j=0;j<SHT->bucket[i].number_of_registries;j++){
      printf("J: %d\n",j);
      memcpy(temp,data2+(j*sizeof(SecondaryRecord)),sizeof(SecondaryRecord));
      printf("%d\n",temp->tupleId);
    }
  }


  free(record);
  free(temp);
  free(SHT->bucket);
  free(SHT);

  if(printed == 0){
    printf("No registry found with surname = %s please try again\n", index_key);
  }

  return HT_OK;
}

HT_ErrorCode SHT_HashStatistics(char *filename ) {
  //insert code here
  int totalBlocks, *sindexDesc, blocknum;
  char *data;
  SecondaryRecord *record;
  record = (SecondaryRecord *) malloc(sizeof(SecondaryRecord));
  BF_Block *block;
  BF_Block_Init(&block);
  Secondary_Directory *SHT;
  SHT = (Secondary_Directory *) malloc(sizeof(Secondary_Directory));

  SHT_OpenSecondaryIndex(filename, sindexDesc);

  CALL_BF(BF_GetBlockCounter(*sindexDesc, &blocknum));

  printf("file %s has a total of %d blocks\n", filename, blocknum);

  int min=100, max=0, sum=0;

  for(int i=0 ; i<Power(2, SHT->global_depth) ; i++){
    if(SHT->bucket[i].number_of_registries < min) min = SHT->bucket[i].number_of_registries;
    if(SHT->bucket[i].number_of_registries > max) max = SHT->bucket[i].number_of_registries;
    sum = sum + SHT->bucket[i].number_of_registries;
  }
  float avg;
  avg = (double) (sum/Power(2, SHT->global_depth));
  printf("Minimum registries in a bucket are: %d, Maximum are: %d and average are: %f\n", min, max, avg);


  return HT_OK;
}

HT_ErrorCode SHT_InnerJoin(int sindexDesc1, int sindexDesc2,  char *index_key ) {
  //insert code here
  return HT_OK;
}


// #endif // HASH_FILE_H