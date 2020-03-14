# gif-viewer
## Simple and lightweight GIF-Viewer written in C

Keep in mind, this is a rather small project so it probably will not work with every GIF file. Most of the GIF files I will test this with were rewritten by [GIMP](https://www.gimp.org/), which exports much cleaner GIF files than most of the ones one will find on the Internet. At least it seems so after I tried to figure out why my parser will not parse to the end of the file with original file, but with GIMP files it works fantastic.

### References
- [An introduction into the GIF file format](https://www.matthewflickinger.com/lab/whatsinagif/bits_and_bytes.asp)
- [Formal overview of the GIF specification](https://www.fileformat.info/format/gif/egff.htm)
- [Summary of the file format](http://www.onicos.com/staff/iz/formats/gif.html)
- [Wikipedia page of the LZW compression algorithm](https://en.wikipedia.org/wiki/Lempel%E2%80%93Ziv%E2%80%93Welch)

This software depends on [SDL2](https://www.libsdl.org/index.php).

You will probably have to use your own build system for now. I am using CMake. I have uploaded my own CMake file, but it may not work with your configuration.
