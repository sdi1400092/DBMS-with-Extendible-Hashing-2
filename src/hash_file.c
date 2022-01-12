#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "hash_file.h"
#include "sht_file.h"
#define MAX_OPEN_FILES 20
#define MAX_SIZE_OF_BUCKET BF_BLOCK_SIZE

#define CALL_BF(call)       \
{                           \
  BF_ErrorCode code = call; \
  if (code != BF_OK) {      \
    BF_PrintError(code);    \
    return HT_ERROR;        \
  }                         \
}

// //a struct for the buckets
// //technically its a struct that stores info on the bucket and a point(number)
// //of the block the data is stored
// typedef struct {
//   int *HashCode;
//   int number_of_block; //με τη παραδοχη οτι 1 block = 1 καδος
//   int number_of_registries;
//   int local_depth;
//   int maxSize;
// }buckets;

// //a struct for the HashTable that consists of an array of buckets
// //the global depth of the table and max buckets is an assistant variable 
// //responsible for storing the max number of struct buckets that can be stored
// //in one block
// typedef struct {
//   buckets *bucket;
//   int global_depth;
//   int max_buckets;
// }HashTable;

//open files is an array of integers responsible for storing all the open files at any time
int Open_files[MAX_OPEN_FILES];

//a function that turns a decimal number to the equivalent number in binary and return an array of the 0s and 1s
void HashFunction(int id, int depth, int **hashing){
  int i, *binary;
  binary = (int *) malloc(depth*sizeof(int));
  for(i=0;i<depth;i++){
    binary[i] = id%2;
    id = id/2;
  }
  *hashing = binary;
}

//a function the takes as an argument a record and prints it
void printRecord(Record record){
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

//power calculates the x^y expression 
int power(int base, int exp){
  int result = 1;
  while(exp!=0){
    result *= base;
    --exp;
  }
  return result;
}

//a function that updates the existing directory in the blocks and allocates any new blocks that are needed
//in case the existing ones area not enough for storing the whole directory
HT_ErrorCode updateDirectory(HashTable *HT, int indexdesc){

  char *data, *data2;
  BF_Block *tempBlock, *tempBlock2;
  BF_Block_Init(&tempBlock);
  BF_Block_Init(&tempBlock2);

  CALL_BF(BF_GetBlock(indexdesc,0,tempBlock));
  data=BF_Block_GetData(tempBlock);
  memcpy(data, &(HT->global_depth), sizeof(int));
  memcpy(data+sizeof(int), &(HT->max_buckets), sizeof(int));

  if(power(2,HT->global_depth)<=HT->max_buckets){
    int i, num_of_buckets=power(2,HT->global_depth), num_of_next_block=-1;
    memcpy(data+2*sizeof(int), &(num_of_buckets), sizeof(int));
    memcpy(data+3*sizeof(int), &(num_of_next_block), sizeof(int));
    for( i=0;i<power(2,HT->global_depth);i++){
      memcpy(data+4*sizeof(int)+(i*sizeof(buckets)),&(HT->bucket[i]),sizeof(buckets));
    }
    BF_Block_SetDirty(tempBlock);
    CALL_BF(BF_UnpinBlock(tempBlock));
  }
  else{
    int i, counter = power(2, HT->global_depth);
    int offset = 0;
    for( i=0;i<HT->max_buckets;i++){
      memcpy(data+4*sizeof(int)+(i*sizeof(buckets)),&(HT->bucket[offset]),sizeof(buckets));
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
      for( i=0;i<HT->max_buckets && counter>0 ; i++){
        memcpy(data2+4*sizeof(int)+(i*sizeof(buckets)),&(HT->bucket[offset]),sizeof(buckets));
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
        while(counter>0 && j<HT->max_buckets){
          memcpy(data2+4*sizeof(int)+(j*sizeof(buckets)), &(HT->bucket[offset]), sizeof(buckets));
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
void getDirectory(HashTable **HT, int indexdesc){  
  int *hashing, i, counter=0, number_of_buckets, next_block;
  char *data, *data2;
  BF_Block *Dirblock;
  BF_Block_Init(&Dirblock);

  BF_GetBlock(indexdesc, 0, Dirblock);
  data = BF_Block_GetData(Dirblock);

  memcpy(&((*HT)->global_depth), data, sizeof(int));
  memcpy(&((*HT)->max_buckets), data+sizeof(int), sizeof(int));
  memcpy(&(number_of_buckets), data+2*sizeof(int), sizeof(int));
  (*HT)->bucket = (buckets *) malloc(power(2,(*HT)->global_depth) * sizeof(buckets));
  int a=0;
  
  while(counter<power(2, (*HT)->global_depth)){
    for(int j=0 ; j<number_of_buckets ; j++){
      memcpy(&((*HT)->bucket[counter]), data+4*sizeof(int)+j*sizeof(buckets), sizeof(buckets));
      HashFunction(counter,(*HT)->global_depth,&((*HT)->bucket[counter].HashCode));
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

//initializing the open files variable
HT_ErrorCode HT_Init() {
  //insert code here 
  int i;
  for(i=0;i<MAX_OPEN_FILES;i++){
    Open_files[i]=-1;   
  } 
  return HT_OK;
}

//creates a file with name = filename and creates an empty directory for this file with starting
//global_depth = depth and 2^depth buckets
HT_ErrorCode HT_CreateIndex(const char *filename, int depth) {
  int id, i, j, n, file_desc, **binary;
  char *data;

  CALL_BF(BF_CreateFile(filename));

  //creating HT and allocating required space for it
  HashTable *HT;
  HT = (HashTable *) malloc(sizeof(HashTable));
  //allocating space for 2^depth buckets
  HT->bucket = (buckets *) malloc(power(2,depth)*sizeof(buckets));
  for(i=0;i<power(2,depth);i++){
    //gives starting values to every bucket
    HT->bucket[i].local_depth = depth;
    HT->bucket[i].maxSize = MAX_SIZE_OF_BUCKET/(sizeof(struct Record));
    HT->bucket[i].number_of_registries = 0;
    n=i;
    HT->bucket[i].HashCode =malloc(depth*sizeof(int));
    for(j=0;j<depth;j++){
      HT->bucket[i].HashCode[j]=n%2;
      n=n/2;
    }
  }
  //giving starting values to HashTable
  HT->global_depth = depth;
  HT->max_buckets= (BF_BLOCK_SIZE-(4*sizeof(int)))/sizeof(buckets);

  //opens file and allocates one block for the directory
  CALL_BF(BF_OpenFile(filename, &file_desc));
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
    HT->bucket[i].number_of_block= block_num -1;
    //setting the blocks for the buckets dirty and upnin them
    BF_Block_SetDirty(temp_block2);
    CALL_BF(BF_UnpinBlock(temp_block2));
  }

  //copying the directory to the first block of the file
  memcpy(data, &(HT->global_depth), sizeof(int));
  memcpy(data+sizeof(int), &(HT->max_buckets), sizeof(int));
  int x;
  x = power(2, HT->global_depth);
  memcpy(data+2*sizeof(int), &x, sizeof(int)); //x is the number of how many buckets are stored in the block
  x = -1;
  memcpy(data+3*sizeof(int), &x, sizeof(int)); //here x is the number of the next block if its -1 it means there's no next block
  for( i=0;i<power(2,HT->global_depth);i++){
    memcpy(data+4*sizeof(int)+(i*sizeof(buckets)),&(HT->bucket[i]),sizeof(buckets));
  }
  //setting dirty that block and upnining it
  BF_Block_SetDirty(temp_block);
  CALL_BF(BF_UnpinBlock(temp_block));

  printf("File: %s is created\n", filename);
  CALL_BF(BF_CloseFile(file_desc));
  free(HT->bucket);
  free(HT);

  return HT_OK;
}

//a function that opens a file with name = filename and return an id at the varaible indexDesc
//and adds the indexDesc of the file to the open files array
HT_ErrorCode HT_OpenIndex(const char *fileName, int *indexDesc){
  //insert code here
  int i;
  int temp;
  CALL_BF(BF_OpenFile(fileName,&temp));
  *indexDesc=temp;
  for(i=0 ; i < MAX_OPEN_FILES ; i++){
    if(Open_files[i] == -1){
      Open_files[i] = *indexDesc;
      break;
    }
  }
  return HT_OK;
}

//closes file with id = indexDesc and remove the indexDesc of the file from the open files array
HT_ErrorCode HT_CloseFile(int indexDesc) {
  //insert code here
  int i;
  for(i=0;i<MAX_OPEN_FILES;i++){
    if (Open_files[i]=indexDesc){
      Open_files[i]=-1;
    }
  }
  CALL_BF(BF_CloseFile(indexDesc));
  return HT_OK;
}

//a function responsible for inserting an entry at the file via the directory
HT_ErrorCode HT_InsertEntry(int indexDesc, Record record, int *tuppleid, UpdateRecordArray **updateArray) {
  //insert code here
  int *hashing, i, offset;
  char *data, *data2;
  Record *temp = &record;
  HashTable *HT;
  HT = (HashTable *) malloc(sizeof(HashTable));
  BF_Block *block;
  BF_Block_Init(&block);

  getDirectory(&HT, indexDesc);

  HashFunction(record.id, HT->global_depth, &hashing);

  //find in which bucket the record we want to insert should go
  int counter;
  for(i=0;i<(power(2,HT->global_depth));i++){
    counter=0;
    for(int j=0;j<(HT->bucket[i].local_depth);j++){
      if (HT->bucket[i].HashCode[j]== hashing[j]){
        counter++;
      }
    }
    if(counter == HT->bucket[i].local_depth){
      break;
    }
  }

  //we get that block from the memory
  CALL_BF(BF_GetBlock(indexDesc, HT->bucket[i].number_of_block, block));
  data2 = BF_Block_GetData(block);
  offset = HT->bucket[i].number_of_registries;
  //if the block has space for the registry we store it in there 
  if (offset < HT->bucket[i].maxSize){
    memcpy(data2 + offset*sizeof(struct Record), temp, sizeof(struct Record));
    BF_Block_SetDirty(block);
    CALL_BF(BF_UnpinBlock(block));
    HT->bucket[i].number_of_registries++;
    int blockId=HT->bucket[i].number_of_block;
    int num_of_rec_in_block= HT->bucket[i].maxSize;
    int index_of_rec_in_block= offset;
    *tuppleid=((blockId+1)*num_of_rec_in_block)+index_of_rec_in_block;
    //after updating the directory we store the updated one in the memory
    updateDirectory(HT, indexDesc);

  }
  //if there's no space in that bucket we have two cases
  else{
    //if the local depth of the bucket is equal to the global depth it means that we need to expand
    if(HT->bucket[i].local_depth == HT->global_depth){
      //expand
      HT->global_depth += 1;
      HT->bucket=(buckets *) realloc(HT->bucket,power(2,HT->global_depth)*sizeof(buckets));
      int z=0;
      for(int j=(power(2,HT->global_depth-1));j<(power(2,HT->global_depth));j+=1){
        HT->bucket[j]=HT->bucket[z];
        z+=1;
      }
      //Hashing
      for(int j=0;j<power(2,HT->global_depth);j++){
        HT->bucket[j].HashCode = (int *) malloc(HT->global_depth*sizeof(int));
        HashFunction(j,HT->global_depth,&(HT->bucket[j].HashCode));
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
    int k=(HT->global_depth)-(HT->bucket[i].local_depth);
    int z=i;
    for(int j=1;j<=power(2,k);j++){
      HT->bucket[z].local_depth+=1;
      z += j*power(2, HT->bucket[i].local_depth-1);
    }
    int blocknum;
    BF_GetBlockCounter(indexDesc, &blocknum);
    z=i;
    for(int j=1;j<=(power(2,k)/2);j++){
      z += j*power(2, HT->bucket[i].local_depth-1);
      HT->bucket[z].number_of_block = blocknum - 1;
      HT->bucket[z].number_of_registries = 0;
    }

    BF_Block_SetDirty(temp_block);
    CALL_BF(BF_UnpinBlock(temp_block));

    int x = HT->bucket[i].number_of_registries;
    HT->bucket[i].number_of_registries = 0;

    updateDirectory(HT, indexDesc);

    //call the insert recursively to reorganize the existing registries of the bucket that got splited
    Record *temp_record;
    temp_record = (Record *) malloc(sizeof(Record));
    UpdateRecordArray *tempRecordArray;
    tempRecordArray = (UpdateRecordArray *) malloc(x * sizeof(UpdateRecordArray));
    int *temp;
    for(int j=0 ; j<x ; j++){
      memcpy(temp_record, data2 + j*sizeof(struct Record), sizeof(struct Record));
      HT_InsertEntry(indexDesc, *temp_record, temp, updateArray);
      strcpy(tempRecordArray[j].surname, temp_record->surname);
      tempRecordArray[j].oldTupleId = ((HT->bucket[i].number_of_block + 1)*HT->bucket[i].maxSize) + j;
      tempRecordArray[j].newTupleId = *temp;
    }
    *updateArray = tempRecordArray;
    free(temp_record);
    //call insert one last time for the original registry
    HT_InsertEntry(indexDesc, record, tuppleid, updateArray);
  }

  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));
  
  free(HT->bucket);
  free(HT);
}

//a function that either prints one record of the file via the directory or prints all of them
//depending on how you call it 
HT_ErrorCode HT_PrintAllEntries(int indexDesc, int *id) {
  //insert code here
  int *id_hashing, i, count=0, *printed, flag;
  Record *record;
  record = (Record *) malloc(sizeof(Record));
  BF_Block *block;
  BF_Block_Init(&block);
  char *data, *records_data;
  HashTable *HT;
  HT = (HashTable *)malloc(sizeof(HashTable));

  //get directory
  getDirectory(&HT, indexDesc);
  
  //if no id was giving at the call of the function print all of the registries
  if(id==NULL){
    int i;
    for(i=0;i<(power(2,HT->global_depth));i++){
      //in case there some bucketes with local depth < global depth this means there are two bucket indexes pointing 
      //in the same block and since when new buckets are allocated their number is increasing and so is the index of
      //of the bucket array if the index is bigger than the number of the block being pointed this means wehave already 
      //printed that block
      if(i<HT->bucket[i].number_of_block){
        BF_GetBlock(indexDesc,HT->bucket[i].number_of_block,block);
        data= BF_Block_GetData(block);
        //print records of block data
        printf("bucket: %d with %d registries\n", HT->bucket[i].number_of_block, HT->bucket[i].number_of_registries);
        for(int j=0 ; j<HT->bucket[i].number_of_registries ; j++){
          memcpy(record, data + j*sizeof(Record), sizeof(Record));
          printRecord(*record);
          count++;
        }
      }
      CALL_BF(BF_UnpinBlock(block));
    }
    printf("count = %d\n", count);
  }
  else{
    //if an id was given find that registry via the directory and print only that one
    printf("id to be found = %d\n", *id);
    HashFunction(*id, HT->global_depth, &id_hashing);
    int counter, block_num;
    for(i=0;i<(power(2,HT->global_depth));i++){
      counter=0;
      for(int j=(HT->bucket[i].local_depth)-1;j>=0;j--){
        if (HT->bucket[i].HashCode[j]== id_hashing[j]){
          counter++;
        }
      }
      if(counter == HT->bucket[i].local_depth){
        break;
      }
    }

    block_num = HT->bucket[i].number_of_block;
    CALL_BF(BF_GetBlock(indexDesc, block_num, block));
    data = BF_Block_GetData(block);
    for(int j=0 ; j<HT->bucket[i].number_of_registries ; j++){
      memcpy(record, data + j*sizeof(Record), sizeof(Record));
      if(record->id == *id){
        printRecord(*record);
      }
    }
    CALL_BF(BF_UnpinBlock(block));
  }

  free(HT->bucket);
  free(HT);

  return HT_OK;
}