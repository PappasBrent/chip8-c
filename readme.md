## Single File CHIP-8 Emulator
A very fast CHIP-8 written in less than 500 source lines of C code.

## Requirements
- [gcc](https://gcc.gnu.org/)
- [make](https://www.gnu.org/software/make/)
- [SDL2](https://www.libsdl.org/)
- [SDL2 Mixer](https://github.com/libsdl-org/SDL_mixer/releases)

## Quickstart
First `make` the emulator:
```bash
$ make
```

Then run it by passing it a CHIP-8 ROM from the command line, i.e.
```bash
$ ./chip8 <ROM>
```
where \<ROM\> is the path to a CHIP-8 ROM.

## Testing
```bash
$ make test
```

You should hear a short beeping sound and see this screen if all the tests pass:
![expected-test-results](expected.png)

## Acknowledgments and References
- Thanks to Cowgod for making the first (as far as I know) publicly available [technical reference](http://devernay.free.fr/hacks/chip8/C8TECH10.HTM)
- I also referred to [James Griffin's implementation](https://github.com/JamesGriffin/CHIP-8-Emulator/) a few times for checking my implementation of some of the more ambiguous instructions (e.g, `Fx55` and `Fx65`) and for a quickstart into using SDL
- Lazy Foo' Productions for [great tutorials on SDL](https://lazyfoo.net/tutorials/SDL/)
- Finally, NinjaWeedle's [fork](https://github.com/NinjaWeedle/chip8-test-rom-with-audio) of corax89's CHIP-8 [test rom](https://github.com/corax89/chip8-test-rom) made it easy (read: possible) to test the audio, video, and logic of my emulator
