build:
	mpicc -o tema3 tema3.c hashtable.c -pthread -Wall
clean:
	rm -rf tema3
