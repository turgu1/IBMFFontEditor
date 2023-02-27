### IBMF Font Editor

This is a simple font editor for the IBMF Font format. 
Using QtCreator v9.0.1 and GCC.

Work in progress. Not ready yet.

ToDo:

- [x] Run Length Encoding algorithm for glyph' bitmap
- [x] Ligature / Kerning format change in IBMFFont file structure
- [x] ibmf_font.hpp update according to new IBMFFont structure format
- [ ] Testing IBMFFont new format - ongoing
- [x] Algorithm for modified font file saved to disk 
- [ ] Kern and Ligature edition and update
- [x] Connect vertical/horizontal scrollBars to grid positioning
- [ ] Add export to .h header file
- [ ] Add manual Text entry and formatting with the current font for proofreading.
- [ ] Add import capability (GFX and/or TTF)
- [ ] Testing everything

Known bugs:

- [x] Changing currently shown face does modify the face metrics shown, but not the glyphs' bitmaps. Corrected.
- [x] Size information for some widgets not updated by Qt at the expected time. The app must be resized manually to get proper values.
- [ ] Glyph Preview widget adjustments required.
- [x] Bitmap renderer (central widget) not positioned properly. The app must be resized to get proper positioning.


#### Main screen capture:

<img src="Pictures/main2.png" alt="app picture" width="500"/>
