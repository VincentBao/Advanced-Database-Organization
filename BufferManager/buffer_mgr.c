//
//  buffer_mgr.c
//  BufferManager
//
//  Created by vincent on 10/24/16.
//
//  Copyright © 2016 Fujie Bao. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>

#include "buffer_mgr.h"
#include "dberror.h"
#include "storage_mgr.h"
#include "dt.h"
#include <math.h>
#include <stdbool.h>



typedef struct Page
{
    PageNumber pn;			//page number of each page
    int recent_hit_time;	// the most recent time the page has been used
    SM_PageHandle data; 	//data stored in each page
    bool dirty; 			//boolean value to check if the page is dirty or not
    int fixed_count;		// number of clients using the page
} PageNode;


int buffer_size = 0; 		//size of the of buffer
int count_read = 0;			//number of pages read from disk
int num_write_IO = 0;		//number of pages write to disk
int hit = 0;				//counting the hit times , can be used in LRU, assigned to the recent_hit_time

int clock_pointer = 0;


void pinPage_helper (BM_BufferPool *const bm, BM_PageHandle *const page,
                     const PageNumber pageNum);
// int CLOCK_pointer = 0;
// int LRU_pointer = 0;


//FIFO function
void FIFO(BM_BufferPool *const bm, PageNode *page){
    PageNode *pageNode = (PageNode *) bm->mgmtData;
    
    int i, head;
    
    head = count_read % buffer_size;
    
    for(i = 0; i < buffer_size; i++){
        
        if(pageNode[head].fixed_count == 0){
            
            if(pageNode[head].dirty == true){
                SM_FileHandle file;
                openPageFile(bm->pageFile, &file);
                writeBlock(pageNode[head].pn, &file, pageNode[head].data);
                
                num_write_IO++;
            }
            
            pageNode[head].data = page->data;
            pageNode[head].dirty = page->dirty;
            pageNode[head].fixed_count = page->fixed_count;
            pageNode[head].pn = page->pn;
            break;
            
        }
        else{
            
            head++;
            if(count_read % buffer_size == 0){
                head = 0;
            }
        }
    }
    
}



//LRU function
void LRU(BM_BufferPool *const bm, PageNode *page){
    PageNode *pageNode = (PageNode *) bm -> mgmtData;
    
    //variables used to find the least recent index
    int i, leastRecentHitIdx, leastRecentHit;
    
    //found the pageNode that has the fixed_count = 0, store the index and get its recent_hit_time
    for(i = 0; i< buffer_size; i++){
        if(pageNode[i].fixed_count == 0){
            leastRecentHitIdx = i;
            leastRecentHit = pageNode[i].recent_hit_time;
            break;
        }
    }
    
    //find the next leastRecentHitIdx after the recent one, using the recent_hit_time stored in each page
    //loop start from the next index of current one
    //find the minimum recent_hit_time and assign it to leastRecentHit
    for(i = leastRecentHitIdx+1; i< buffer_size; i++){
        if(pageNode[i].recent_hit_time < leastRecentHit){
            leastRecentHitIdx = i;
            leastRecentHit = pageNode[i].recent_hit_time;
        }
    }
    
    //check if the page we found is dirty and write to disk
    if(pageNode[leastRecentHitIdx].dirty == true){
        SM_FileHandle smfh;
        openPageFile(bm->pageFile, &smfh);
        writeBlock(pageNode[leastRecentHitIdx].pn, &smfh, pageNode[leastRecentHitIdx].data);
        num_write_IO++;  //increate the write counter
    }
    
    //assign content of new page to this one
    pageNode[leastRecentHitIdx].data = page->data;
    pageNode[leastRecentHitIdx].pn = page->pn;
    pageNode[leastRecentHitIdx].dirty = page->dirty;
    pageNode[leastRecentHitIdx].fixed_count = page -> fixed_count;
    pageNode[leastRecentHitIdx].recent_hit_time = page -> recent_hit_time;
    
    
}

void CLOCK(BM_BufferPool *bm, PageNode *page){
    
    PageNode *pageframe;
    
    pageframe = (PageNode *)bm->mgmtData;
    
    for(clock_pointer = 0; clock_pointer < buffer_size; clock_pointer++){
        
        if(clock_pointer % buffer_size == 0){
            clock_pointer = 0;
        }
        
        if(pageframe[clock_pointer].recent_hit_time == 0){
            
            if(pageframe[clock_pointer].dirty == 1){
                SM_FileHandle *fh;
                openPageFile(bm->pageFile, fh);
                writeBlock(pageframe[clock_pointer].pn, fh, pageframe[clock_pointer].data);
                num_write_IO++;
            }
            
            pageframe[clock_pointer].data = page->data;
            pageframe[clock_pointer].dirty = page->dirty;
            pageframe[clock_pointer].fixed_count = page->fixed_count;
            pageframe[clock_pointer].pn = page->pn;
            pageframe[clock_pointer].recent_hit_time = page->recent_hit_time;
            
            break;
        }
        
        else{
            pageframe[clock_pointer++].recent_hit_time = 0;
        }
    }
}


// Buffer Manager Interface Pool Handling
RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName,
                  const int numPages, ReplacementStrategy strategy,
                  void *stratData){
    int i;
    
    bm->numPages = numPages;
    bm->pageFile = (char *)pageFileName;
    bm->strategy = strategy;
    
    
    // reserve space for the page
    PageNode *page = malloc(sizeof(PageNode) * numPages);
    
    //set buffer size to numPages
    buffer_size = numPages;
    
    
    //init all pages in the pool
    for(i = 0; i < buffer_size; i++){
        page[i].data = NULL;
        page[i].pn = -1;
        page[i].fixed_count = 0;
        page[i].dirty = false;
        page[i].recent_hit_time = 0;
        
    }
    
    bm->mgmtData = page;
    num_write_IO = 0;
    
    return RC_OK;
    
    
    
}


// shut down the buffer pool, remove all pages from it and free the memory
RC shutdownBufferPool(BM_BufferPool *const bm){
    
    
    PageNode *pageNode = (PageNode *)bm -> mgmtData;
    
    forceFlushPool(bm);
    
    int i;
    for(i = 0; i< buffer_size; i++){
        
        //page has been modifed and not yet written to disk
        if(pageNode[i].fixed_count != 0){
            return RC_BUFFERPINNED_PAGE;
        }
    }
    
    
    //releace space and return RC_OK
    free(pageNode);
    bm->mgmtData = NULL;
    return RC_OK;
    
}


//write all dirty pages to disk
RC forceFlushPool(BM_BufferPool *const bm){
    
    PageNode *pageNode = (PageNode *) bm->mgmtData;
    
    int i;
    
    for(i = 0; i < buffer_size; i++){
        
        if(pageNode[i].fixed_count == 0 && pageNode[i].dirty == true){
            SM_FileHandle smfh;
            openPageFile(bm->pageFile, &smfh);						//open page file
            writeBlock(pageNode[i].pn, &smfh, pageNode[i].data); 	//write block of data to the page file
            pageNode[i].dirty = false; 								//mark dirty
            
            num_write_IO++;
        }
    }
    
    return RC_OK;
}



// Buffer Manager Interface Access Pages


//mark a page to dirty
RC markDirty (BM_BufferPool *const bm, BM_PageHandle *const page){
    
    int i;
    
    PageNode *pageNode = bm->mgmtData;
    
    //mark all the dirty page in the pool
    for(i = 0; i < bm->numPages; i++){
        if(pageNode[i].pn == page->pageNum){
            pageNode[i].dirty = true;
            return RC_OK;
        }
    }
    
    return RC_ERROR;
}


//unpin a page from memory
RC unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page){
    int i;
    
    PageNode *pageNode = bm->mgmtData;
    
    for(i = 0; i < bm->numPages; i++){
        
        //decrease the fixed_count for an unpined page
        if(pageNode[i].pn == page->pageNum){
            pageNode[i].fixed_count--;
            break;
        }
    }
    
    return RC_OK;
}


//write the content of modified page back to the page file
RC forcePage (BM_BufferPool *const bm, BM_PageHandle *const page){
    
    int i;
    
    PageNode *pageNode = bm->mgmtData;
    
    for(i = 0; i < buffer_size; i++){
        
        //find target page
        if(pageNode[i].pn == page->pageNum){
            
            SM_FileHandle smfh;
            openPageFile(bm->pageFile, &smfh);
            writeBlock(pageNode[i].pn, &smfh, pageNode[i].data);
            
            // mark not dirty because the modified page has been writter to disk
            pageNode[i].dirty = false;
            
            //increate write count
            num_write_IO++;
        }
    }
   	return RC_OK;
}



//pin a page with pagenumber

RC pinPage (BM_BufferPool *const bm, BM_PageHandle *const page,
            const PageNumber pageNum){
    
    PageNode *pageNode = (PageNode *)bm -> mgmtData;
    
    //check if the pool is empty
    //first page to be pinned
    if(pageNode[0].pn == -1)
    {
        //read page from disk and intialize
        SM_FileHandle fh;
        openPageFile(bm->pageFile, &fh);
        pageNode[0].data = (SM_PageHandle) malloc(PAGE_SIZE);
        ensureCapacity(pageNum,&fh);
        readBlock(pageNum, &fh, pageNode[0].data);
        pageNode[0].pn = pageNum;
        pageNode[0].fixed_count++;
        count_read = hit = 0;
        pageNode[0].recent_hit_time = hit;
        page->pageNum = pageNum;
        page->data = pageNode[0].data;
        
        return RC_OK;
    }
    
    else{
        
        int i;
        bool bufferFull = true;
        
        for(i = 0; i < buffer_size; i++)
        {
            // check if the page in buffer
            if(pageNode[i].pn != -1)
            {
                if(pageNode[i].pn == pageNum)
                {
                    //increase fixed count meaning one more user using it
                    pageNode[i].fixed_count++;
                    bufferFull = false;
                    
                    //use to record recent_hit_time in LRU
                    hit++;
                    
                    if(bm->strategy == RS_LRU)
                        // assign it to recent_hit_time for LRU usage
                        pageNode[i].recent_hit_time = hit;
                    else if(bm->strategy == RS_CLOCK)
                        // assign recent_hit+time to 1 for CLOCK usage
                        pageNode[i].recent_hit_time = 1;
                    
                    page->pageNum = pageNum;
                    page->data = pageNode[i].data;
                    
                    break;
                }
            }
            
            // when pagenumber is -1
            else {
                SM_FileHandle fh;
                openPageFile(bm->pageFile, &fh);
                pageNode[i].data = (SM_PageHandle) malloc(PAGE_SIZE);
                readBlock(pageNum, &fh, pageNode[i].data);
                pageNode[i].pn = pageNum;
                pageNode[i].fixed_count = 1;
                
                count_read++;
                
                //use to record recent_hit_time in LRU
                hit++;
                
                if(bm->strategy == RS_LRU)
                    // assign it to recent_hit_time for LRU usage
                    pageNode[i].recent_hit_time = hit;
                else if(bm->strategy == RS_CLOCK)
                    pageNode[i].recent_hit_time = 1;
                
                page->pageNum = pageNum;
                page->data = pageNode[i].data;
                
                bufferFull = false;
                break;
            }
        }
        
        if (bufferFull == true){
            pinPage_helper(bm, page, pageNum);
        }
        return RC_OK;
        
    }
    
}



// a helper function, use it when bufferfull is true
void pinPage_helper (BM_BufferPool *const bm, BM_PageHandle *const page,
                     const PageNumber pageNum){
    
    
    //create a new page
    PageNode *newPage = (PageNode *) malloc(sizeof(PageNode));
    SM_FileHandle smfh;
    openPageFile(bm->pageFile, &smfh);
    newPage->data = (SM_PageHandle) malloc(PAGE_SIZE);
    readBlock(pageNum, &smfh, newPage->data);
    newPage->pn = pageNum;
    newPage->dirty = false;
    newPage->fixed_count = 1;
    count_read++;
    hit++;
    
    if(bm-> strategy == RS_LRU)
        // assign it to recent_hit_time for LRU usage
        newPage->recent_hit_time = hit;
    else if(bm->strategy == RS_CLOCK)
        newPage->recent_hit_time = 1;
    
    
    page->pageNum = pageNum;
    page->data = newPage->data;
    
    //call functions according to stratagy
    switch(bm->strategy){
        case RS_FIFO:
            FIFO(bm, newPage);
            break;
            
        case RS_LRU:
            LRU(bm, newPage);
            break;
            
        case RS_CLOCK:
            printf("\n CLOCK algorithm not implemented");
            break;
            
        case RS_LFU: // Using LFU algorithm
            printf("\n LFU algorithm not implemented");
            break;
            
        case RS_LRU_K:
            printf("\n LRU-k algorithm not implemented");
            break;
            
        default:
            printf("\n Algorithm Not Implemented\n");
            break;
            
    }
}




// Statistics Interface


//returns an array of page numbers
PageNumber *getFrameContents (BM_BufferPool *const bm){
    PageNumber *frameContents = malloc(sizeof(PageNumber) * buffer_size);
    PageNode *pageNode = (PageNode *) bm->mgmtData;
    
    int i;
    for (i = 0; i< buffer_size; i++){
        if(pageNode[i].pn != -1){
            frameContents[i] = pageNode[i].pn;
        }
        else{
            frameContents[i] = NO_PAGE;
        }
    }
    return frameContents;
    
}


//returns an array of bools indicating the dirty status
bool *getDirtyFlags (BM_BufferPool *const bm){
    
    bool *dirtyPages = malloc(sizeof(bool) * buffer_size);
    PageNode *pageNode = (PageNode *)bm -> mgmtData;
    
    int i;
    //loop and set all the page and assign to the return array
    for(i=0; i<buffer_size; i++){
        dirtyPages[i] = pageNode[i].dirty;
    }
    
    return dirtyPages;
}


//returns an array of integers of number of users
int *getFixCounts (BM_BufferPool *const bm){
    
    int *fixed_counts = malloc (sizeof(int) * buffer_size);
    int i;
    
    PageNode *pageNode = (PageNode *)bm->mgmtData;
    fixed_counts = malloc(sizeof(int) * buffer_size);
    
    for(i=0; i< buffer_size; i++){
        if(pageNode[i].fixed_count != -1){
            fixed_counts[i] = pageNode[i].fixed_count;
        }
        else{
            fixed_counts[i] = 0;
        }
    }
    
    return fixed_counts;
    
}

//returns number of pages has been read
int getNumReadIO(BM_BufferPool *const bm){
    //make sure the count is greater than zero
    return count_read+1;
}

//returns number of pages has been write
int getNumWriteIO (BM_BufferPool *const bm){
    return num_write_IO;
}
