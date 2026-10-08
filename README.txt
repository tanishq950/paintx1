COMPUTER GRAPHICS PAINT PROGRAM
================================

Files
-----
main.cpp    Native Windows paint application using C++ and Win32 GDI.
index.html  Project documentation page.
styles.css  Styling for the documentation page.

Features
--------
* Pencil/freehand drawing
* Straight lines
* Rectangles and ellipses
* Text tool
* Eraser
* 12 colors
* Brush size from 1 to 50 pixels
* Undo and redo (up to 30 states)
* Clear canvas
* Save as BMP

Build with Visual Studio Developer Command Prompt
---------------------------------------------------
cl /EHsc /DUNICODE /D_UNICODE main.cpp user32.lib gdi32.lib comdlg32.lib /link /SUBSYSTEM:WINDOWS

Build with MinGW g++
--------------------
g++ -std=c++17 -municode -mwindows main.cpp -o paint.exe -luser32 -lgdi32 -lcomdlg32

Important
---------
This implementation intentionally uses no JavaScript or third-party libraries.
The HTML/CSS files document the project; the interactive paint program is the
native C++ executable, which is suitable for a CGR desktop project.
