#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "storage_mgr.h"
#include "dberror.h"
#include "test_helper.h"

// test name
char *testName;

/* test output files */
#define TESTPF "test_pagefile.bin"

/* prototypes for test functions */
static void testCreateOpenClose(void);
static void testMultiPageContent(void);

/* main function running all tests */
int
main (void)
{
  testName = "";
  
  initStorageManager();

  testCreateOpenClose();
  testMultiPageContent();

  return 0;
}


/* check a return code. If it is not RC_OK then output a message, error description, and exit */
/* Try to create, open, and close a page file */
void
testCreateOpenClose(void)
{
  SM_FileHandle fh;

  testName = "test create open and close methods";

  TEST_CHECK(createPageFile (TESTPF));
  
  TEST_CHECK(openPageFile (TESTPF, &fh));
  ASSERT_TRUE(strcmp(fh.fileName, TESTPF) == 0, "filename correct");
  ASSERT_TRUE((fh.totalNumPages == 1), "expect 1 page in new file");
  ASSERT_TRUE((fh.curPagePos == 0), "freshly opened file's page position should be 0");

  TEST_CHECK(closePageFile (&fh));
  TEST_CHECK(destroyPageFile (TESTPF));

  // after destruction trying to open the file should cause an error
  ASSERT_TRUE((openPageFile(TESTPF, &fh) != RC_OK), "opening non-existing file should return an error.");

  TEST_DONE();
}

/* Try to create, open, and close a page file */
void
testMultiPageContent(void)
{
  SM_FileHandle fh;
  SM_PageHandle ph;
  int i;

  testName = "test multi-page content";

  ph = (SM_PageHandle) calloc(PAGE_SIZE,sizeof(char));


  // create a new page file
  TEST_CHECK(createPageFile (TESTPF));
  TEST_CHECK(openPageFile (TESTPF, &fh));
  printf("created and opened file\n");
  
  // // read first page into handle
  // TEST_CHECK(readFirstBlock (&fh, ph));
  // // the page should be empty (zero bytes)
  // for (i=0; i < PAGE_SIZE; i++)
  //   ASSERT_TRUE((ph[i] == 0), "expected zero byte in first page of freshly initialized page");
  // printf("first block was empty\n");
    
  // // change ph to be a string and write that one to disk
  // for (i=0; i < PAGE_SIZE; i++)
  //   ph[i] = (i % 10) + '0';
  // TEST_CHECK(writeBlock (0, &fh, ph));
  // printf("writing first block\n");

  // read back the page containing the string and check that it is correct
  //memset(ph, '0', PAGE_SIZE);
  TEST_CHECK(readFirstBlock (&fh, ph));
  ASSERT_TRUE((fh.curPagePos == 0), "after readFirstBlock (read first page) page position should be 0");
  for (i=0; i < PAGE_SIZE; i++)
    ASSERT_TRUE((ph[i] == 0), "character in page read from disk is the one we expected.");
  printf("reading first block\n");



// ************* Testing more functions *************

  //1.  append empty block and check the number of pages
  TEST_CHECK(appendEmptyBlock(&fh));
  ASSERT_TRUE((fh.totalNumPages == 2), "Expected 2 pages after appended ");


  // //2. read current page into handle
  // TEST_CHECK(readCurrentBlock (&fh, ph));

  // ASSERT_TRUE((fh.curPagePos == 0), "after readCurrentBlock (read first page) page position should be 0");
  // printf("read current (first) page\n");
  // //  page should be empty (zero bytes)
  // for (i=0; i < PAGE_SIZE; i++)
  //     ASSERT_TRUE((ph[i] == 0), "Expected 0 byte in the new page");
  // printf("reading current block\n");


  // //3. change ph to be a string and write it on current page
  // for (i=0; i < PAGE_SIZE; i++)
  //     ph[i] = (i % 10) + '0';
  // TEST_CHECK(writeCurrentBlock (&fh, ph));
  // printf("writing to the current block \n");
  
  //4. ensure page capacity, using number 6 as example 
  TEST_CHECK(ensureCapacity (6, &fh));
  printf("%d\n",fh.totalNumPages);
  ASSERT_TRUE((fh.totalNumPages == 6), "Expect 6 pages after ensureCapacity with 6");

  //confirm if pages appended sucessfully
  ASSERT_TRUE((fh.curPagePos == 6), "current page position move to the lastest page: 6");
 
  // //5. read previous block
  // TEST_CHECK(readPreviousBlock (&fh, ph));
  // for (i=0; i < PAGE_SIZE; i++)
  //     ASSERT_TRUE((ph[i] == (i % 10) + '0'), "character read from disk is expected");
  // printf("reading the previous block with written data\n");
  

  // //6. read next page into handle
  // TEST_CHECK(readNextBlock (&fh, ph));
  // //  page should be empty (zero bytes)
  // for (i=0; i < PAGE_SIZE; i++)
  //     ASSERT_TRUE((ph[i] == 0), "Expected 0 byte in latest appended page");
  // printf("reading the next block which was appended before. Expected empty\n");

  
  //7. read last page into handle
  TEST_CHECK(readLastBlock (&fh, ph));
  // make sure the page is empty, 0 bytes
  for (i=0; i < PAGE_SIZE; i++)
      ASSERT_TRUE((ph[i] == 0), "Expected 0 byte in the new page");
  printf("reading the last block that was appended before. Expected empty\n");

// close new page file 
TEST_CHECK(closePageFile (&fh));
// destroy new page file
TEST_CHECK(destroyPageFile (TESTPF));  
free(ph);  
TEST_DONE();
}
