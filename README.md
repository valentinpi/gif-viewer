# gif-viewer
## Simple and lightweight GIF-Viewer written in C

This software depends on [SDL2](https://www.libsdl.org/index.php). It is licensed with the MIT license.

You will probably have to use your own build system for now. I am using CMake. I have uploaded my own CMake file, but it may not work with your configuration.

Some of the examples I painted myself were made using [GIMP](https://www.gimp.org/). For the other examples and the [specification text](https://www.w3.org/Graphics/GIF/spec-gif89a.txt) I do not claim copyright. These files are only included for educational purposes.

### TODO
- Implement interlacing
- Fully parse all the extension blocks, right now there is only support for transparency and duration specified in the graphics control blocks
- Modularize the gif components, specifying a more clear interface with the library

### References
- [An introduction into the GIF file format](https://www.matthewflickinger.com/lab/whatsinagif/bits_and_bytes.asp)
- [Formal overview of the GIF specification](https://www.fileformat.info/format/gif/egff.htm)
- [Summary of the file format](http://www.onicos.com/staff/iz/formats/gif.html)
- [Wikipedia page of the LZW compression algorithm](https://en.wikipedia.org/wiki/Lempel%E2%80%93Ziv%E2%80%93Welch)
- [A breakdown of a GIF decoder](https://commandlinefanatic.com/cgi-bin/showarticle.cgi?article=art011)
