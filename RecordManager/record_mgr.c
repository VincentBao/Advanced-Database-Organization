//
//  record_mgr.c
//  RecordManager
//
//  Created by vincent on 11/14/16.
//
//  Copyright © 2016 Fujie Bao. All rights reserved.
//

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "buffer_mgr.h"
#include "storage_mgr.h"

#include "dberror.h"
#include "record_mgr.h"
#include "tables.h"
#include "expr.h"
#include "record_mgr.h"

int nextpage = 1;


RM_TableData * CurrentTable;  //initical one table as the current table 

//custom struct for table information
typedef struct TableInfo
{
    int numTuples;           // total number of tuples 
    int nextpage;            // next page
    BM_BufferPool *bp;       // buffer pool
    SM_FileHandle fh;        // file handle
    
} TableInfo;

//custom struct for scan usage
typedef struct ScanInfo
{
    int slot;      //current position for scanning
    int page;      //current page of scanning
    Expr *cond;    //conditions of record
}ScanInfo;


//initial function
RC initRecordManager (void *mgmtData)   
{
    printf("******** Initializing Record Manager **********\n");
    return RC_OK;
}

//shut down record manager
RC shutdownRecordManager ()            
{
    printf("********* Shutting Down Record Manager ************\n");
    return RC_OK;
}


//create table function
extern RC createTable (char *name, Schema *schema)
{
    //allocate space for buffer pool and page handle
    BM_BufferPool *bp = (BM_BufferPool *) malloc (sizeof(BM_BufferPool));
    BM_PageHandle *ph = (BM_PageHandle *) malloc (sizeof(BM_PageHandle)); 

    //get the record size and remaining page 
    int recordSize = getRecordSize(schema);
    int pageRemain = (PAGE_SIZE - sizeof(int))/(recordSize + sizeof(int));

    //create page file
    createPageFile(name);   
    SM_FileHandle fh;

    //initial table struct
    TableInfo *tableInfo;
    

    //opening file and initializing buffer pool
    openPageFile(name, &fh); 
    initBufferPool(bp, name, 100, RS_FIFO, NULL);
    
    int tuple_number = 0;
    nextpage = 1;

      
    pinPage(bp, ph, 0);
    

    //assign data to current table
    int *tableDataInfo = (int*)ph->data;
    tableDataInfo[0] = tuple_number;
    tableDataInfo[1] = nextpage;
    Schema *table_schema = (Schema *)(tableDataInfo + 2 * sizeof(int));
    table_schema = schema;


    markDirty(bp, ph);
    unpinPage(bp, ph);
    

    CurrentTable = (RM_TableData*)malloc(sizeof(RM_TableData));
    CurrentTable->name = name;
    CurrentTable->schema = schema;
    CurrentTable->mgmtData = malloc(sizeof(TableInfo));
    
    
    tableInfo=(TableInfo*)CurrentTable->mgmtData;


    //initial table files
    tableInfo->numTuples=0;
    tableInfo->nextpage=1;
    tableInfo->bp=bp;
    tableInfo->fh=fh;


    //append the first record page
    appendEmptyBlock(&fh);      
      

    //initial a new page handle for storing slot information
    BM_PageHandle *new_ph = (BM_PageHandle *) malloc (sizeof(BM_PageHandle)); 
    pinPage(bp, new_ph, 1);                      
    int *pageInfo = (int *)new_ph->data;
    pageInfo[0] = 0;                                                 
    
    int *slotInfo = (int *)(new_ph->data + sizeof(int));
    int i;
    for(i = 0; i < pageRemain; i++ ){
        slotInfo[i] = 0;
    }

 
    markDirty(bp, new_ph);
    unpinPage(bp, new_ph);
    
    return RC_OK;
    
}


//open table function
RC openTable (RM_TableData *rel, char *name){
    
    //use the current table we initialized 
    rel->name = CurrentTable->name;    
    rel->schema = CurrentTable->schema;    
    rel->mgmtData = CurrentTable->mgmtData;
    
    return RC_OK;
    
}



//close table function
RC closeTable (RM_TableData *rel){
    
    //set everything to NULL
    rel->name = NULL;     
    rel->mgmtData = NULL;       
    
    return RC_OK;
}


//delete table function
RC deleteTable (char *name){

    //free the space for current table
    free(CurrentTable);         
    return RC_OK;
}


//get number of tuples
int getNumTuples (RM_TableData *rel){
    return ((TableInfo *)rel->mgmtData)->numTuples;   
}


//get record size
int getRecordSize (Schema *schema){
    int size = 0;
    int i;
    DataType *dataType;
    
    dataType = schema->dataTypes;
    for (i = 0; i < schema -> numAttr; i++) {
        switch(dataType[i]){
            case DT_STRING:
                size += (schema->typeLength[i]) * sizeof(char);
                break;
            
            case DT_INT:
                size += sizeof(int);
                break;
                
            case DT_FLOAT:
                size += sizeof(float);
                break;
                
            case DT_BOOL:
                size += sizeof(bool);
                break;
                
            default:
                break;
        }        
    }
    return size;
}



//insert record function
RC insertRecord (RM_TableData *rel, Record *record){
    
    //allocate space for table info and page handle
    TableInfo *t = (TableInfo *)rel->mgmtData;
    BM_PageHandle *page = (BM_PageHandle *) malloc (sizeof(BM_PageHandle));


    //get record size and remaining page
    int recordSize = getRecordSize(rel->schema);
    int pageRemain = (PAGE_SIZE - sizeof(int))/(recordSize + sizeof(int));
    

    //number of free slot that can be used, -1 as initial value
    int freeSlot = -1;        

    //first avaliable page
    int first_page =((TableInfo *)rel->mgmtData)->nextpage;     

    //pin this page
    pinPage(t->bp, page, first_page);      

    //finding free slot
    int state = 0;
    int index = 0;
    char *offset = page->data + sizeof(int);
    int i;
    for (i = 0; i < pageRemain; i++) { 
        state = *(int *)offset; 
        if (state == 0) {    
            // 0 for empty, 1 for used
            break;
        }
        offset+= sizeof(int); 
        index += 1;
    }


    //page is full
    if(index > pageRemain-1){      
        index = -1;
    }

    //assign result to freeSlot
    freeSlot = index;
    

    //if empty page exist
    if(freeSlot >= 0){
        
        int offset = PAGE_SIZE  - (freeSlot + 1) * recordSize * sizeof(char)-1;
        
        //relocate record
        record->id.slot = freeSlot;      
        record->id.page = first_page;
        
        //insert this record
        memcpy(page->data + offset, record, recordSize);   
        

        //update the id of slot
        int * slot_header = (int *)(page->data + sizeof(int));      
        slot_header[freeSlot] = 1;
        

        //update number of tuples
        int *page_tuples_num = (int *)page->data;      
        page_tuples_num[0] += 1;
        
        
        markDirty(t->bp, page);
        unpinPage(t->bp, page);

        // update page 0 
        pinPage(t->bp, page, 0);      
        
        int *recordNum = (int *)page->data;
        //increment page record number
        recordNum[0] += 1;        
        
        markDirty(t->bp, page);
        unpinPage(t->bp, page);
        

        //increment number of tuples
        ((TableInfo *)rel->mgmtData)->numTuples += 1;      
    }
    

    //more than 1 avaliable 
    else if (freeSlot < 0) {        
        
        markDirty(t->bp, page);        
        unpinPage(t->bp, page);
         
        //append a new page    
        appendEmptyBlock(&((TableInfo *)rel->mgmtData)->fh);        
        
        //reset a new first avaliable page
        int new_first_page = ((TableInfo *)rel->mgmtData)->nextpage + 1;      
        int UseableSlot = 0;
        
        BM_BufferPool *bp =((TableInfo *)rel->mgmtData)->bp;       
        BM_PageHandle *ph_c = (BM_PageHandle *) malloc (sizeof(BM_PageHandle));

        //pin the new page
        pinPage(bp, ph_c, new_first_page);
        

        //intiial the new record page
        int *PAGE_RECORD_INFO = (int *)ph_c->data;     
        PAGE_RECORD_INFO[0] = 0;

        //initial the slot info
        int *slotID = (int *)(ph_c->data + sizeof(int));       
        int j;
        for(j = 0; j < pageRemain; j++){
            slotID[j] = 0;
        }
    
    
        int offset = PAGE_SIZE  - recordSize * sizeof(char)-1;
        

        //relocate the record
        record->id.slot = 0;   
        record->id.page = new_first_page;
        
        //insert the record
        memcpy(ph_c->data + offset, record, recordSize);     
        
        // update the slotID
        int * slot_header = (int *)(ph_c->data + sizeof(int));      
        slot_header[UseableSlot] = 1;
        
        //increment the number of tuple
        int *page_tuples_num = (int *)ph_c->data;      
        page_tuples_num[0] += 1;
        
        
        markDirty(bp, ph_c);
        unpinPage(bp, ph_c);


        //pin the page 0
        pinPage(t->bp, page, 0);  
        
        int *recordNum = (int *)page->data;

        //increment page record number
        recordNum[0] += 1;    
        recordNum[1]+=1;    
        
        markDirty(t->bp, page);
        unpinPage(t->bp, page);
        

        //update table info
        t->numTuples += 1;      
        t->nextpage = new_first_page;
    }
   
   
    free(page);
    return RC_OK;    
}


//delete record function
RC deleteRecord (RM_TableData *rel, RID id){


    //retrive data and allocate space for page handle and table info
    BM_PageHandle *page1 = (BM_PageHandle*)malloc(sizeof(BM_PageHandle));
    TableInfo *t = (TableInfo*)rel->mgmtData;

    char *record = (char*)malloc(getRecordSize(rel->schema) * sizeof(char));
    
    //pin page according to id
    pinPage(t->bp, page1, id.page);
    
    int length = getRecordSize(rel->schema) * sizeof(char); 

    //empty this record
    memset(record, 0, length);

    char *temp = page1->data + PAGE_SIZE  - (id.slot + 1) * getRecordSize(rel->schema) 
            * sizeof(char)-1;


    memcpy(temp, record, getRecordSize(rel->schema));
    
    ((int*)(page1->data))[id.slot + 1] = 0;
    ((int*)(page1->data))[0] = ((int*)(page1->data))[0] - 1;  
    
    markDirty(t->bp, page1);
    unpinPage(t->bp, page1);
    pinPage(t->bp, page1, 0);
    
    ((int*)(page1->data))[0] = ((int*)(page1->data))[0] - 1;
    
    markDirty(t->bp, page1);
    unpinPage(t->bp, page1);
    
    t->numTuples = t->numTuples - 1;
    
    free(page1);
    return RC_OK;

}


//update record function
RC updateRecord (RM_TableData *rel, Record *record){
    
    //allocate space for page handle and table info
    BM_PageHandle *page1 = (BM_PageHandle *) malloc (sizeof(BM_PageHandle));
    TableInfo *t = (TableInfo *)rel->mgmtData;
    
    pinPage(t->bp, page1, record->id.page);
    
    char *temp = page1->data + PAGE_SIZE  - (record->id.slot + 1) * 
                getRecordSize(rel->schema) * sizeof(char)-1;
    memcpy(temp,record, getRecordSize(rel->schema));    
         
    ((int *)(page1->data))[record->id.slot+1] = 1;
    
    markDirty(t->bp, page1);
    unpinPage(t->bp, page1);
    
    free(page1);  
    return RC_OK; 
}


//get record function
RC getRecord (RM_TableData *rel, RID id, Record *record){


    //allocate space for page handle and table info
    BM_PageHandle *page1;
    TableInfo *t = (TableInfo *)rel->mgmtData;
    
    page1 = (BM_PageHandle *) malloc (sizeof(BM_PageHandle));
    
    pinPage(t->bp, page1, id.page);     
    
    int offset = PAGE_SIZE  - (id.slot + 1) * getRecordSize(rel->schema) 
                * sizeof(char)-1;

    char *temp= page1->data + PAGE_SIZE  - (id.slot + 1) * 
            getRecordSize(rel->schema) * sizeof(char) - 1;
    
    //record->temp
    memcpy( record,temp ,getRecordSize(rel->schema));      
    
    markDirty(t->bp, page1);
    unpinPage(t->bp, page1);
    
    free(page1);
    
    return RC_OK;
}


// create schema function
Schema *createSchema (int numAttr, char **attrNames, DataType *dataTypes, int *typeLength, int keySize, int *keys){
    
    //initialize a new schema
    Schema *res =(Schema *)malloc(sizeof(Schema));
    
    //assign the paremeter into the new schema
    res->attrNames=attrNames;       
    res->dataTypes=dataTypes;       
    res->typeLength=typeLength;    
    res->keySize=keySize;      
    res->keyAttrs=keys;    
    res->numAttr=numAttr;      
    
    return  res;
    
    
}


//start scan function
RC startScan (RM_TableData *rel, RM_ScanHandle *scan, Expr *cond)
{

    //  initial scanning process
    ScanInfo *ScanInfo=malloc(sizeof(ScanInfo));      
    ScanInfo->cond=cond;
    ScanInfo->slot=0;
    ScanInfo->page=1;
    
    //assign parameters to scan struct
    scan->mgmtData=ScanInfo;            
    scan->rel=rel;
    
    return RC_OK;
}


//next function
RC next (RM_ScanHandle *scan, Record *record)
{

    //doing scanning, store results in *record
    RID rid;     
    Value *result=malloc(sizeof(Value));
    int count,tuple_number,i;
    int slot_number;
  
    count=((ScanInfo *)scan->mgmtData)->slot;
    rid.page=((ScanInfo *)scan->mgmtData)->page;
    rid.slot=count;
    
    tuple_number=((TableInfo *)scan->rel->mgmtData)->numTuples;
    slot_number=(PAGE_SIZE -sizeof(int))/(getRecordSize(scan->rel->schema)+sizeof(int));
    
                                           
    for (i=count;i<tuple_number;i++)
    {
        
        //if a page is already read, increment to next page
        if (rid.slot>=slot_number)     
        {
            rid.page++;
            rid.slot= -1;
        }
                
        //else point to next
        getRecord(scan->rel, rid, record);
        evalExpr(record, scan->rel->schema, ((ScanInfo *)scan->mgmtData)->cond, &result);
        

        //check if result is true, quit loop
        if (result->v.boolV)                
        {
            break;
        }
        rid.slot++;
    }
            
    if (i>=tuple_number)
    {
        return RC_RM_NO_MORE_TUPLES;
    }


    //increment to next slot
    i++;

    //store i to slot
    ((ScanInfo *)scan->mgmtData)->slot=i;
    
    
    return RC_OK;
}


RC closeScan (RM_ScanHandle *scan)
{
    free(scan->mgmtData);
    return RC_OK;
}

//free schema
RC freeSchema (Schema *schema)
{
    free(schema);
    return RC_OK;
}


//create record function
RC createRecord (Record **record, Schema *schema)
{
    Record *record1=(Record *)malloc(sizeof(Record));     
    *record=record1;
    record1->data=(char *) malloc (getRecordSize(schema));
    return RC_OK;
}


//free record
RC freeRecord (Record *record)
{
    free(record);
    return RC_OK;
}

//get attribute
RC getAttr (Record *record, Schema *schema, int attrNum, Value **value)
{
   
    Value *temp_value=(Value *)malloc(sizeof(Value));
    int offset=0,i;
    DataType *datatype=schema->dataTypes;

    //calculate offset to each attribute
    for (i = 0; i < attrNum; i++)
    {
        switch (datatype[i])
        {
            case DT_STRING:
            {
                offset += (schema->typeLength[i]) * sizeof(char);
                break;
            }
            case DT_INT:
            {
                offset += sizeof(int);
                break;
            }
            case DT_FLOAT:
            {
                offset += sizeof(float);
                break;
            }
            case DT_BOOL:
            {
                offset += sizeof(bool);
                break;
            }
            default:
            {
                break;
            }
        }
        
    }

    //find the value of the attribute
    switch (datatype[attrNum])
    {
        case DT_STRING:
        {
            //allocate space to the string data type
            temp_value->v.stringV=(char *)calloc(schema->typeLength[attrNum],sizeof(char));
            memcpy(temp_value->v.stringV,((char *)record->data)+offset,schema->typeLength[attrNum]*sizeof(char));
            break;
        }
        case DT_INT:
        {
            memcpy(&(temp_value->v.intV),record->data+offset,sizeof(int));
            break;
        }
        case DT_FLOAT:
        {
            memcpy(&(temp_value->v.floatV),record->data+offset,sizeof(float));
            break;
        }
        case DT_BOOL:
        {
            memcpy(&(temp_value->v.boolV),record->data+offset,sizeof(bool));
            break;
        }
        default:
            break;
    }
    temp_value->dt=datatype[attrNum];
    *value=temp_value;
    return RC_OK;
}


//set attribute function
RC setAttr (Record *record, Schema *schema, int attrNum, Value *value)
{
    int length,i;
    length=0;
    
    //calculate length/offset
    for (i = 0; i < attrNum; i++)
    {
        switch(schema->dataTypes[i]){
            case DT_STRING:
                length += (schema->typeLength[i]) * sizeof(char);
                break;
                
            case DT_INT:
                length+= sizeof(int);
                break;
                
            case DT_FLOAT:
                length += sizeof(float);
                break;
            
            case DT_BOOL:
                length += sizeof(bool);
                break;
            default:
                break;
        }
        
    }
    switch (value->dt)
    {
        case DT_STRING:
        {
            strcpy(record->data+length,value->v.stringV);
            break;
        }
        case DT_INT:
        {
            memcpy(record->data+length,&(value->v.intV),sizeof(int));
            break;
        }
        case DT_FLOAT:
        {
            memcpy(record->data+length,&(value->v.floatV),sizeof(float));
            break;
        }
        case DT_BOOL:
        {
            memcpy(record->data+length,&(value->v.boolV),sizeof(bool));
            break;
        }
        default:
            break;

    }
    return RC_OK;
}
