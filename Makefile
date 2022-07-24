SRC=main.cc
TARGET=main

make:	$(SRC)
	g++ -o $(TARGET) $(SRC) -lSDL2

clean:
	rm -f $(TARGET)