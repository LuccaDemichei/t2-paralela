CC = mpicc
CFLAGS = -Wall -O2
TARGET = AverageFilterKernel
SRC = AverageFilterKernel.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET) filtered.pgm

run: $(TARGET)
	mpirun -np 4 ./$(TARGET) SaltAndPepperNoise.pgm 3

.PHONY: all clean run

