# icon_maker
This is a tool to create small icons or pixel art from an image. It can load a custom color palette from a file  
and automatically map the colors from the input image to the palette colors.  
A color reuse cost can be specified to prevent the tool from reusing the same colors too much.  
Input colors that have a good match in the palette are mapped first, and other colors are mapped to the  
nearest match while avoiding excessive color reuse.  
Options to scale the input image before mapping are available (intended to downscale for icons), as well  
as an option to upscale post-mapping by an integer factor to give the output a pixelated look.  

Custom palettes can be provided in a text file with one RGB color per line (a default is included).  
Currently only png images are supported.  

## How do I compile it?
Use the provided makefile. On linux just run "make" in the project folder, no fancy flags needed.  

## How do I use this thing?
First just run icon_maker with no arguments, it will tell you what arguments it needs.  
Try giving it a png image with the default settings and look at what it does, then play with the options.  
A few color palettes are included in the project folder, you may want to start with db32.txt.  
Leave the -SHIFT option for last, you probably will not need it (changes the color reuse cost normalization).  
