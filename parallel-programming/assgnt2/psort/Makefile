CC=nvcc

all: psort

psort: psort.o
	$(CC) -o psort psort.o

psort1.o: psort.cu
	$(CC) -o psort.o -c psort.cu

clean:
	rm -f psort.o psort
