CC=nvcc

all: vadd

vadd: vadd.o
	$(CC) -o vadd vadd.o

vadd.o: vadd.cu
	$(CC) -o vadd.o -c vadd.cu

clean:
	rm -f vadd.o vadd
