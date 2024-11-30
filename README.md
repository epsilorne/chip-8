# CHIP-8 Emulator
An emulator (though technically, an interpreter) for the CHIP-8 language, written in C.

## Building
```
git clone https://github.com/epsilorne/chip-8.git
cd chip-8
make
```

## Usage
`./chip8 <rom.ch8> [freq]`

`rom.ch8` is the name of the ROM you want to load.

`freq` is the CPU's frequency in Hz and can be changed to suit the specific ROM. By default, it is 500Hz.

## Keyboard
CHIP-8 keys are mapped - based on a QWERTY layout - as follows:
| CHIP-8 Key | Physical Key |
| ---------- | ------------ |
| `1 2 3 C`<br>`4 5 6 D`<br>`7 8 9 E`<br>`A 0 B F` | `1 2 3 4`<br>`Q W E R`<br>`A S D F`<br>`Z X C V` |

## Customisation
If you wish to customise the colours of the emulator, you may do so by changing the values of `fg_colour` and `bg_colour` in `chip8.c` before recompiling again. Three bits are reserved for each of the red and green channels, and two bits for the blue channel.

## References
[Guide to making a CHIP-8 emulator](https://tobiasvl.github.io/blog/write-a-chip-8-emulator)
