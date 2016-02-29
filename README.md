# bfc83p
An optimizing [BF](https://esolangs.org/wiki/Brainfuck) compiler that runs on a TI-83+ series calculator.

## Compilation (Linux only)
To compile bfc83p, you need [SDCC](http://sdcc.sourceforge.net/) and [binpac8x](http://www.ticalc.org/archives/files/fileinfo/429/42915.html).
You will also need to download the [c_ti83p](https://github.com/mortonsc/c_ti83p) library.

To download and compile bfc83p:

    git clone https://github.com/mortonsc/bfc83p.git
    cd bfc83p
    git clone https://github.com/mortonsc/c_ti83p.git lib
    make

## Implementation
bfc83p implements BF with 8-bit cells. Overflow or underflow causes the cell value to wrap, so `[-]` and `[+]`
are identical and both set the value of the current cell to 0. Memory is not wrapping:
the memory pointer starts at the beginning of a 768-byte block of RAM, with every cell set to 0.
It is perfectly possible to leave this region from either end
and enter memory that may be in use by the calculator; behavior in this case is undefined.
Unmatched opening or closing brackets will cause a compile-time error.

Output is implemented as text output to the screen, and input as input from the keyboard. The numerical representations
for text characters and keypresses generally do not agree outside of capital letters.

It is theoretically possible, with great care, for a program compiled with bfc83p to execute arbitrary machine code.
Figuring out how to do this is left as an exercise for the reader.

While a compiled program is running, pressing [ON] at any time will terminate it and generate an `ERR: BREAK`
message, just as with TI-BASIC. This is a safety feature in case your program gets stuck in an infinite loop.

## Optimization
bfc83p implements a few simple optimizations that significantly reduce the size and running time of the compiled program.
These include:
  * Multiple consecutive + or - replaced with a single addition or subtraction from the current cell
  * Multiple consecutive < or > replaced with a single addition or subtraction from the memory pointer
  * [-] and [+] simply clear the current cell. If followed by a series of + or -, clearing and adding are combined
  into a single load operation.
  * [>] and [<] are replaced by scan left/scan right for zero cell
  
## Usage
At the present, bfc83p always looks for its input in a program called `BFSRC`, and always writes its output to a new program
called `BFDST`. The output program can be run as any assembly program.
