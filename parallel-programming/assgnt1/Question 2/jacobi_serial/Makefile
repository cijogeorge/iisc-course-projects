FILE=~/programs/jacobi_serial/jacobi_serial
CC=/usr/share/mpich2/bin/mpicc

all: $(FILE)

$(FILE): $(FILE).o
	$(CC) -o $(FILE) $(FILE).o

$(FILE).o: $(FILE).c
	$(CC) -o $(FILE).o -c $(FILE).c

clean:
	rm -f $(FILE).o $(FILE)
