SRC=main.c
TARGET=chip8

$(TARGET):	$(SRC)
	gcc -o $(TARGET) $(SRC) -lSDL2

test:	$(TARGET)
	./$(TARGET) ./test_roms/test_opcode.ch8

clean:
	rm -f $(TARGET)