CC = mpicc
TARGET = AverageFilterKernel
SRC = AverageFilterKernel.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET) filtered.pgm

run: $(TARGET)
	mpirun -np 4 ./$(TARGET) SaltAndPepperNoise.pgm 3

.PHONY: all clean run

