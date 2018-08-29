'Doom style' renderer learning exercise / test. C++/SDL2/Emscripten.

Here's my version of Bisqwit's QB vertrot (vertex rotation) test app ported to C/SDL, with many improvements & compiled to WebAssembly.

This means it runs in the browser:
http://morganrobertson.net/wasm/renderer.html

Here's Bisqwit's full renderer in the browser:
http://morganrobertson.net/wasm/bisqwit.html

Bisqwit's original video that started this quest for knowledge:
https://www.youtube.com/watch?v=HQYsFshbkYw

After I watched this video I looked at the qbasic code and was surprised by the simplicity of rendering a pseudo 3d first person perspective. 

The problem was that I had no idea about any of the math.  I hope this post provides some leads to a viewer like myself who didn't know where to start.

** ABSOLUTE VIEW: ** 

The first step was drawing the 'absolute view'.  It's fairly straight forward except for the line with the COS & SIN function.  This is used to draw the direction of the player's view based on an angle controlled by the keys.  I knew about COS & SIN from triangles in school but didn't understand why it's used here.  The answer to this is the 'unit circle'.  If you read how SIN & COS relate to the unit circle, you'll see how this math works.


** TRANSFORM VIEW: **

The next tricky part is the lines under the comment 'Rotate them (the vertices) around the player's view'.  This part I spent considerable hours on.  I couldn't understand why he was using both SIN & COS to rotate the line, why he was assigning to a 'Z' coordinate considering this view is still 2D and why are both X & Y values used to generate this Z value.  I started off trying all sorts of rubbish like drawing triangles from the player's position to the line vertices (read: line ends) but it wasn't shining any light on the topic. 

The secret answer here is the '2d rotation formula' (or more formally '2d rotation matrix') to generate these values.  You can learn about linear algebra for further details.

Although the 2d rotation matrix stuff works it's not exactly intuitive.  This made a lot more sense when I tried taking one point and trying to rotate that around the player's position.  Start off by thinking about drawing an arc in front of the player to the angle of where the player is looking (set in the 'absolute view').  Try to iterate from here.

The reason the 'Z' value is used even though it's relevant in this view is that it IS the Z value (depth) in the next view.


** PERSPECTIVE VIEW: **

The main magic here is the 'perspective divide' where you take the X & Y coordinates of a vertex in 'world space' and divide them by the distance from the player to the object (i.e. Depth / 'Z value').  This automagically converts co-ordinates from the 'world view' to the 'camera/players' view.

In the QB test, the 'walls' (really, they are just lines) are 2 pixels tall (y value) when standing a depth of '1' (unit/pixel) away from the wall.  The wall's width (x values) are just the lengths from the 'transformed view' multiplied by 16.  Why 16? I'm not entirely sure.  It adjusts the 'field of view' somewhat.  In Bisqwit's more complete version of the renderer he uses a 'yscale' & 'xscale' value that is based on the height of the display window.  I'm not entirely sure how he came up with the values he's used for the scalers but they work.

Additionally, I spent more hours looking into the 'intersect' and 'vxp' (cross product) functions he used for the clipping.  The intersection function will return the intersection point of the wall 'line' and a line that goes to either the left or right of the player (simplified this a bit).  This is used to clip (read: make shorter) a wall line if one end of it has a positive Z value (i.e. in front of the player) and the other end has a negative Z value (i.e. behind the player).  The reason for this is that the perspective divide (divide by Z) math does not work out if you have a negative Z value, so you prevent any lines from having a negative Z value (i.e. going behind the player) by clipping them just before they go behind you.

If you have a look at the intersection math, you'll wonder 'how the heck?!' does that function calculate the intersection of two lines.  The secret is that VXP is not actually the VXP, it's really a function for the 'determinate' of a matrix.  Watch 3blue1brown's videos on linear algebra for a better understanding.  If you wanted to nitpick, the cross product of a 2d matrix is the same as the determinate of a 2d matrix so the math works out.  The method for using the determinate to calculate the intersection of two lines is detailed on this wikipedia page:
https://en.wikipedia.org/wiki/Line%E2%80%93line_intersection

Something that is not discussed on the wikipedia page but is found in other texts is that this function works by leveraging something called 'Cramer's rule'.  See here for more:
http://mathworld.wolfram.com/CramersRule.html

There are a few ways to calculate line/line intersection including the 'slope + y intercept' form you learned in school or something called 'parametric' form.

That's it! Math is amazing :)

** Compiling: **

Compiles under VS2017, GCC or Emscripten.  Just need SDL2 + SDL2TTF.

** Emscripten example: **

em++ -O3 -std=c++11 -s USE_SDL=2 -s USE_SDL_TTF=2 --preload-file font.ttf -o render_test.html --shell-file shell-minimal.html -s WASM=1 Main.cpp Utils.cpp

DEBUG:
em++ -std=c++11 -s USE_SDL=2 -s USE_SDL_TTF=2 --preload-file font.ttf -o render_test.html --shell-file shell-minimal.html -s WASM=1 Main.cpp Utils.cpp -g4

** Credits: ** 

- Font taken from KISS SDL project: https://github.com/actsl/kiss_sdl - Thanks a million.
- Bisqwit: The crazy, smart, funny person that inspired this journey.
