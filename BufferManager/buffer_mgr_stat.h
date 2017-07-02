//
//  buffer_mgr_stat.h
//  BufferManager
//
//  Created by vincent on 10/24/16.
//  Copyright © 2016 Fujie Bao. All rights reserved.
//

#ifndef BUFFER_MGR_STAT_H
#define BUFFER_MGR_STAT_H

#include "buffer_mgr.h"

// debug functions
void printPoolContent (BM_BufferPool *const bm);
void printPageContent (BM_PageHandle *const page);
char *sprintPoolContent (BM_BufferPool *const bm);
char *sprintPageContent (BM_PageHandle *const page);

#endif

