SRC=main.c
TARGET=chip8

$(TARGET):	$(SRC)
	gcc -o $(TARGET) $(SRC) -lSDL2 -lSDL2_mixer

test:	$(TARGET)
	./$(TARGET) ./test_roms/chip8-test-rom-with-audio.ch8

clean:
	rm -f $(TARGET)