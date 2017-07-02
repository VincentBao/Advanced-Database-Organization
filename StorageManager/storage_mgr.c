#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "storage_mgr.h"
#include "dberror.h"




void initStorageManager (void){
    printf("initial storage manager......");
}

RC createPageFile (char *fileName){
    
    FILE *fp;
    fp = fopen(fileName, "w+");
    if(fp == NULL){
        return RC_FILE_HANDLE_NOT_INIT;
        //exit(0);
    }
    
    SM_PageHandle firstPage = (SM_PageHandle) calloc (1, PAGE_SIZE * sizeof(char));
    if(firstPage == NULL){
        puts("page cannot be allocated");
    }
    
    if(fwrite(firstPage, sizeof(char), PAGE_SIZE, fp) != PAGE_SIZE){
        return RC_WRITE_FAILED;
    }
    
    free(firstPage);
    
    fclose(fp);
    
    return RC_OK;
    
}

RC openPageFile (char *fileName, SM_FileHandle *fHandle){
    long offset;
    
    FILE *fp;
    fp = fopen(fileName, "r+");
    if(fp == NULL){
        return RC_FILE_NOT_FOUND;
    }
    
    fseek(fp, 0L, SEEK_END);
    
    offset = ftell(fp);
    
    fseek(fp, 0L, SEEK_SET);
    
    fHandle->fileName = fileName;
    fHandle->totalNumPages = ((int)offset + 1) / PAGE_SIZE;
    fHandle->curPagePos = 0;
    fHandle->mgmtInfo = fp;
    
    return RC_OK;
}

RC closePageFile (SM_FileHandle *fHandle){
    if(fclose(fHandle->mgmtInfo) == 0){
        return RC_OK;
    }else{
        return RC_FILE_HANDLE_NOT_INIT;
    }
}

RC destroyPageFile (char *fileName){
    if(remove(fileName) == 0){
        return RC_OK;
    }else{
        return RC_FILE_HANDLE_NOT_INIT;
    }
}

/* reading blocks from disc */
/*The method reads the pageNumth block from a file and stores its content in the memory
 * pointed to by the memPage page handle. If the file has less than pageNum pages,
 * the method should return RC_READ_NON_EXISTING_PAGE*/
RC readBlock (int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage){
    if(fHandle->totalNumPages < pageNum + 1){
        return RC_READ_NON_EXISTING_PAGE;
    }
    
    if(fseek(fHandle->mgmtInfo, pageNum * PAGE_SIZE * sizeof(char), SEEK_SET) != 0){
        return RC_READ_NON_EXISTING_PAGE;
    }
    
    if(fread(memPage, sizeof(char), PAGE_SIZE, fHandle->mgmtInfo) != PAGE_SIZE){
        return RC_READ_NON_EXISTING_PAGE;
    }
    
    fHandle->curPagePos = pageNum;
    
    return RC_OK;
}

int getBlockPos (SM_FileHandle *fHandle){
    
    return fHandle->curPagePos;
}

RC readFirstBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    
    return readBlock(0, fHandle, memPage);
}

RC readPreviousBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    
    int pageNum;
    
    if(getBlockPos(fHandle) == 0){
        return RC_READ_NON_EXISTING_PAGE;
    }else{
        pageNum = fHandle->curPagePos - 1;
        return readBlock(pageNum, fHandle, memPage);
    }
    
}

RC readCurrentBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    
    return readBlock(getBlockPos(fHandle), fHandle, memPage);
}

RC readNextBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    
    int pageNum;
    
    if(pageNum >= fHandle->totalNumPages){
        return RC_READ_NON_EXISTING_PAGE;
    }else{
        pageNum = fHandle->curPagePos + 1;
        return readBlock(pageNum, fHandle, memPage);
    }
}

RC readLastBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    return readBlock(fHandle->totalNumPages-1, fHandle, memPage);
}

/* writing blocks to a page file */
RC writeBlock (int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage){
    if(pageNum >= fHandle->totalNumPages){
        return RC_READ_NON_EXISTING_PAGE;
    }
    
    if(fseek(fHandle->mgmtInfo, pageNum * sizeof(char) * PAGE_SIZE, SEEK_SET) != 0){
        return RC_WRITE_FAILED;
    }
    
    if(fwrite(memPage, sizeof(char), PAGE_SIZE, fHandle->mgmtInfo) != PAGE_SIZE){
        return RC_WRITE_FAILED;
    }
    
    fHandle->curPagePos = pageNum;
    
    return RC_OK;
}

RC writeCurrentBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    return writeBlock(fHandle->curPagePos, fHandle, memPage);
}

/*Increase the number of pages in the file by one.
 * The new last page should be filled with zero bytes
 */
RC appendEmptyBlock (SM_FileHandle *fHandle){
    
    SM_PageHandle newPage = (SM_PageHandle) calloc (PAGE_SIZE, sizeof(char));
    
    if(newPage == NULL){
        return RC_FILE_HANDLE_NOT_INIT;
    }
    
    fseek(fHandle->mgmtInfo, 0L, SEEK_END);
    
    if(fwrite(newPage, sizeof(char), PAGE_SIZE, fHandle->mgmtInfo) != PAGE_SIZE){
        return RC_WRITE_FAILED;
    }
    
    fHandle->totalNumPages++;
    fHandle->curPagePos = fHandle->totalNumPages;
    
    free(newPage);
    
    return RC_OK;
}

/*If the file has less than numberOfPages pages
 * then increase the size to numberOfPages.
 */
RC ensureCapacity (int numberOfPages, SM_FileHandle *fHandle){
    int offset;
    
    if(fHandle->totalNumPages < numberOfPages){
        offset = numberOfPages - fHandle->totalNumPages;
        
        SM_PageHandle increasePage = (SM_PageHandle) calloc (PAGE_SIZE, offset * sizeof(char));
        
        if(increasePage == NULL){
            return RC_FILE_HANDLE_NOT_INIT;
        }
        
        if(fseek(fHandle->mgmtInfo, 0L, SEEK_END) != 0){
            return RC_READ_NON_EXISTING_PAGE;
        }
        
        if(fwrite(increasePage, offset * sizeof(char), PAGE_SIZE, fHandle->mgmtInfo) != PAGE_SIZE){
            return RC_WRITE_FAILED;
        }
        
        fHandle->totalNumPages = numberOfPages;
        fHandle->curPagePos = fHandle->totalNumPages;
        
        free(increasePage);
        
        
    }
    return RC_OK;
}
