Objects = dberror.o storage_mgr.o

all: ass1_1 ass1_2

ass1_2: dberror.o storage_mgr.o test_assign1_2.o
	gcc -o ass1_2 $(Objects) test_assign1_2.o

test_assign1_2.o: test_assign1_2.c test_helper.h dberror.h storage_mgr.h
	gcc -c test_assign1_2.c

ass1_1: dberror.o storage_mgr.o test_assign1_1.o
	gcc -o ass1_1 $(Objects) test_assign1_1.o

test_assign1_1.o: test_assign1_1.c test_helper.h dberror.h storage_mgr.h
	gcc -c test_assign1_1.c

dberror.o: dberror.c dberror.h
	gcc -c dberror.c

storage_mgr.o: storage_mgr.c dberror.h storage_mgr.h
	gcc -c storage_mgr.c

clean:
	rm ass1_1 ass1_2 $(Objects) test_assign1_1.o test_assign1_2.o