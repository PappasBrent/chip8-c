SRC=main.c
TARGET=main

make:	$(SRC)
	gcc -o $(TARGET) $(SRC) -lSDL2

clean:
	rm -f $(TARGET)