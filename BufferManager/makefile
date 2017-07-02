Objects = buffer_mgr.o buffer_mgr_stat.o dberror.o storage_mgr.o 

default: ass2_1 ass2_2

all: ass2_1 ass2_2

ass2_1: test_assign2_1.o buffer_mgr.o dberror.o storage_mgr.o buffer_mgr_stat.o
	gcc -o ass2_1 $(Objects) test_assign2_1.o

ass2_2: test_assign2_2.o buffer_mgr.o dberror.o storage_mgr.o buffer_mgr_stat.o
	gcc -o ass2_2 $(Objects) test_assign2_2.o

test_assign2_1.o: test_assign2_1.c buffer_mgr_stat.h buffer_mgr.h test_helper.h dberror.h storage_mgr.h
	gcc -c test_assign2_1.c

test_assign2_2.o: test_assign2_2.c buffer_mgr_stat.h buffer_mgr.h test_helper.h dberror.h storage_mgr.h
	gcc -c test_assign2_2.c

buffer_mgr_stat.o: buffer_mgr_stat.c buffer_mgr_stat.h buffer_mgr.h
	gcc -c buffer_mgr_stat.c

buffer_mgr.o: buffer_mgr.c dberror.h dt.h storage_mgr.h
	gcc -c buffer_mgr.c

storage_mgr.o: storage_mgr.c storage_mgr.h
	gcc -c storage_mgr.c

dberror.o: dberror.c dberror.h
	gcc -c dberror.c

clean:
	rm $(Objects) test_assign2_1.o test_assign2_2.o