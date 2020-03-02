# gif-viewer
## Simple and lightweight GIF-Viewer written in C

Keep in mind, this is a rather small project so it probably will not work with every GIF file. Most of the GIF files I will test this with were rewritten by [GIMP](https://www.gimp.org/), which exports much cleaner GIF files than most of the ones one will find on the Internet. At least it seems so after I tried to figure out why my parser will not parse to the end of the file with original file, but with GIMP files it works fantastic.

### References
- [An overview of the GIF file format specification, very formal](https://www.fileformat.info/format/gif/egff.htm)
- [An informal and well explained introduction into the GIF file format](https://www.matthewflickinger.com/lab/whatsinagif/bits_and_bytes.asp)
- [A good reference for the LZW decompression algorithm](https://www.matthewflickinger.com/lab/whatsinagif/lzw_image_data.asp)
- [An example for a GIF decoder](https://commandlinefanatic.com/cgi-bin/showarticle.cgi?article=art011)
- [An explicit overview of the file format](http://www.onicos.com/staff/iz/formats/gif.html)

This program depends on [SDL2](https://www.libsdl.org/index.php)
You will probably have to use your own build system for now. I am using CMake. I have uploaded my own CMake file, but it may not work with your configuration.
