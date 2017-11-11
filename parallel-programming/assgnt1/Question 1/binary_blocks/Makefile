FILE=~/programs/binary_blocks/binary_blocks
CC=/usr/share/mpich2/bin/mpicc

all: $(FILE)

$(FILE): $(FILE).o
	$(CC) -o $(FILE) $(FILE).o

$(FILE).o: $(FILE).c
	$(CC) -o $(FILE).o -c $(FILE).c

clean:
	rm -f $(FILE).o $(FILE)
