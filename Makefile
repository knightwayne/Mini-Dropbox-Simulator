PORT=54096

CFLAGS= -DPORT=$(PORT) -g -Wall 

all : dbserver dbclient

dbserver : dbserver.o filedata.o wrapsock.o writen.o readn.o
	gcc ${CFLAGS} -o $@ $^

dbclient : dbclient.o filedata.o wrapsock.o writen.o readn.o
	gcc ${CFLAGS} -o $@ $^

testfiledata : testfiledata.o filedata.o
	gcc ${CFLAGS} -o $@ $^
    
%.o : %.c
	gcc ${CFLAGS} -c $<

clean: 
	rm *.o
