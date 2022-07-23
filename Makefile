SRC=main.cc
TARGET=main

make:	$(SRC)
	g++ -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)