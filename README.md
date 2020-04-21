A 3D wireframe renderer based on the scratchpixel.com simple shader demo code.
Pretty much all I'm doing is simple face normal ratio face culling and instead of filling polys I'm just connecting the vertexes. This is a proof-of-concept for a future vector display program.

compile using:
c++ wireframe.cpp -o wireframe -std=c++11

and then open the .svg file in a web browser.
