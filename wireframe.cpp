//[header]
// A practical implementation of the rasterization algorithm.
//[/header]
//[compile]
// Download the raster3d.cpp, cow.h and geometry.h files to the same folder.
// Open a shell/terminal, and run the following command where the files are saved:
//
// c++ -o raster3d raster3d.cpp  -std=c++11 -O3
//
// Run with: ./raster3d. Open the file ./output.png in Photoshop or any program
// reading PPM files.
//[/compile]
//[ignore]
// Copyright (C) 2012  www.scratchapixel.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//[/ignore]

#include "geometry.h"
#include <fstream>

#include "boat.h"

static const float inchToMm = 25.4;
enum FitResolutionGate { kFill = 0, kOverscan };

//[comment]
// Compute screen coordinates based on a physically-based camera model
// http://www.scratchapixel.com/lessons/3d-basic-rendering/3d-viewing-pinhole-camera
//[/comment]
void computeScreenCoordinates(
    const float &filmApertureWidth,
    const float &filmApertureHeight,
    const uint32_t &imageWidth,
    const uint32_t &imageHeight,
    const FitResolutionGate &fitFilm,
    const float &nearClippingPLane,
    const float &focalLength,
    float &top, float &bottom, float &left, float &right
)
{
    float filmAspectRatio = filmApertureWidth / filmApertureHeight;
    float deviceAspectRatio = imageWidth / (float)imageHeight;
    
    top = ((filmApertureHeight * inchToMm / 2) / focalLength) * nearClippingPLane;
    right = ((filmApertureWidth * inchToMm / 2) / focalLength) * nearClippingPLane;

    // field of view (horizontal)
    float fov = 2 * 180 / M_PI * atan((filmApertureWidth * inchToMm / 2) / focalLength);
    std::cerr << "Field of view " << fov << std::endl;
    
    float xscale = 1;
    float yscale = 1;
    
    switch (fitFilm) {
        default:
        case kFill:
            if (filmAspectRatio > deviceAspectRatio) {
                xscale = deviceAspectRatio / filmAspectRatio;
            }
            else {
                yscale = filmAspectRatio / deviceAspectRatio;
            }
            break;
        case kOverscan:
            if (filmAspectRatio > deviceAspectRatio) {
                yscale = filmAspectRatio / deviceAspectRatio;
            }
            else {
                xscale = deviceAspectRatio / filmAspectRatio;
            }
            break;
    }
    
    right *= xscale;
    top *= yscale;
    
    bottom = -top;
    left = -right;
}

//[comment]
// Compute vertex raster screen coordinates.
// Vertices are defined in world space. They are then converted to camera space,
// then to NDC space (in the range [-1,1]) and then to raster space.
// The z-coordinates of the vertex in raster space is set with the z-coordinate
// of the vertex in camera space.
//[/comment]
void convertToRaster(
    const Vec3f &vertexWorld,
    const Matrix44f &worldToCamera,
    const float &l,
    const float &r,
    const float &t,
    const float &b,
    const float &near,
    const uint32_t &imageWidth,
    const uint32_t &imageHeight,
    Vec3f &vertexRaster
)
{
    Vec3f vertexCamera;

    worldToCamera.multVecMatrix(vertexWorld, vertexCamera);
    
    // convert to screen space
    Vec2f vertexScreen;
    vertexScreen.x = near * vertexCamera.x / -vertexCamera.z;
    vertexScreen.y = near * vertexCamera.y / -vertexCamera.z;
    
    // now convert point from screen space to NDC space (in range [-1,1])
    Vec2f vertexNDC;
    vertexNDC.x = 2 * vertexScreen.x / (r - l) - (r + l) / (r - l);
    vertexNDC.y = 2 * vertexScreen.y / (t - b) - (t + b) / (t - b);

    // convert to raster space
    vertexRaster.x = (vertexNDC.x + 1) / 2 * imageWidth;
    // in raster space y is down so invert direction
    vertexRaster.y = (1 - vertexNDC.y) / 2 * imageHeight;
    vertexRaster.z = -vertexCamera.z;
}

float min3(const float &a, const float &b, const float &c)
{ return std::min(a, std::min(b, c)); }

float max3(const float &a, const float &b, const float &c)
{ return std::max(a, std::max(b, c)); }

float edgeFunction(const Vec3f &a, const Vec3f &b, const Vec3f &c)
{ return (c[0] - a[0]) * (b[1] - a[1]) - (c[1] - a[1]) * (b[0] - a[0]); }

const uint32_t imageWidth = 800;
const uint32_t imageHeight = 600;
// | c00 c01 c02 c03 | -> x-axis
// | c10 c11 c12 c13 | -> y-axis
// | c20 c21 c22 c23 | -> z-axis
// | c30 c31 c32 c33 | -> translation
// represented by worldToCamera
const Matrix44f worldToCamera = {0.707107, -0.331295, 0.624695, 0, 0, 0.883452, 0.468521, 0, -0.707107, -0.331295, 0.624695, 0, -1.63871, -3.747777, -15.400412, 1};


const float nearClippingPLane = 1;
const float farClippingPLane = 1000;
float focalLength = 20; // in mm
// 35mm Full Aperture in inches
float filmApertureWidth = 0.980;
float filmApertureHeight = 0.735;


int main(int argc, char **argv)
{
    Matrix44f cameraToWorld = worldToCamera.inverse();

    // compute screen coordinates
    float t, b, l, r;
    
    computeScreenCoordinates(
        filmApertureWidth, filmApertureHeight,
        imageWidth, imageHeight,
        kOverscan,
        nearClippingPLane,
        focalLength,
        t, b, l, r);
    
    // define the frame-buffer and the depth-buffer. Initialize depth buffer
    // to far clipping plane.
    Vec3<unsigned char> *frameBuffer = new Vec3<unsigned char>[imageWidth * imageHeight];
    for (uint32_t i = 0; i < imageWidth * imageHeight; ++i) frameBuffer[i] = Vec3<unsigned char>(255);
    float *depthBuffer = new float[imageWidth * imageHeight];
    for (uint32_t i = 0; i < imageWidth * imageHeight; ++i) depthBuffer[i] = farClippingPLane;

    // [comment]
    // Store the result of the framebuffer to a SVG file - can open in web browser.
    // [/comment]
    std::ofstream ofs;
    ofs.open("./wireframe.svg");
    ofs << "<svg version=\"1.1\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" xmlns=\"http://www.w3.org/2000/svg\" width=\"" << imageWidth << "\" height=\"" << imageHeight << "\">" << std::endl;
    
    
    // [comment]
    // Outer loop
    // [/comment]
    for (uint32_t i = 0; i < ntris; ++i) {
        // const Vec3f &v0 = vertices[nvertices[i * 3]];
        // const Vec3f &v1 = vertices[nvertices[i * 3 + 1]];
        // const Vec3f &v2 = vertices[nvertices[i * 3 + 2]];
        Vec3f &v0 = vertices[nvertices[i * 3]];
        Vec3f &v1 = vertices[nvertices[i * 3 + 1]];
        Vec3f &v2 = vertices[nvertices[i * 3 + 2]];
        
        float xRadians = 0.0;
        float yRadians = 2.0;
        float zRadians = 0.0;
        float sx = sin(xRadians);
        float sy = sin(yRadians);
        float sz = sin(zRadians);
        float cx = cos(xRadians);
        float cy = cos(yRadians);
        float cz = cos(zRadians);
        
        const Matrix44f mRotateX = {1, 0, 0, 0, 0, cx, -sx, 0, 0, sx, cx, 0, 0, 0, 0, 1};
        const Matrix44f mRotateY = {cy, 0, sy, 0, 0, 1, 0, 0, -sy, 0, cy, 0, 0, 0, 0, 1};
        const Matrix44f mRotateZ = {cz, -sz, 0, 0, sz, cz, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        
        Vec3f vr0, vr1, vr2;
        mRotateY.multVecMatrix(v0, vr0);
        mRotateY.multVecMatrix(v1, vr1);
        mRotateY.multVecMatrix(v2, vr2);
        
        // [comment]
        // Convert the vertices of the triangle to raster space
        // [/comment]
        Vec3f v0Raster, v1Raster, v2Raster;
        convertToRaster(vr0, worldToCamera, l, r, t, b, nearClippingPLane, imageWidth, imageHeight, v0Raster);
        convertToRaster(vr1, worldToCamera, l, r, t, b, nearClippingPLane, imageWidth, imageHeight, v1Raster);
        convertToRaster(vr2, worldToCamera, l, r, t, b, nearClippingPLane, imageWidth, imageHeight, v2Raster);
        
        // [comment]
        // Precompute reciprocal of vertex z-coordinate
        // [/comment]
        v0Raster.z = 1 / v0Raster.z,
        v1Raster.z = 1 / v1Raster.z,
        v2Raster.z = 1 / v2Raster.z;
    
        float xmin = min3(v0Raster.x, v1Raster.x, v2Raster.x);
        float ymin = min3(v0Raster.y, v1Raster.y, v2Raster.y);
        float xmax = max3(v0Raster.x, v1Raster.x, v2Raster.x);
        float ymax = max3(v0Raster.y, v1Raster.y, v2Raster.y);
        
        // the triangle is out of screen
        if (xmin > imageWidth - 1 || xmax < 0 || ymin > imageHeight - 1 || ymax < 0) continue;

        // be careful xmin/xmax/ymin/ymax can be negative. Don't cast to uint32_t
        uint32_t x0 = std::max(int32_t(0), (int32_t)(std::floor(xmin)));
        uint32_t x1 = std::min(int32_t(imageWidth) - 1, (int32_t)(std::floor(xmax)));
        uint32_t y0 = std::max(int32_t(0), (int32_t)(std::floor(ymin)));
        uint32_t y1 = std::min(int32_t(imageHeight) - 1, (int32_t)(std::floor(ymax)));

        float area = edgeFunction(v0Raster, v1Raster, v2Raster);
        
        // [comment]
        // Inner loop
        // [/comment]
        for (uint32_t y = y0; y <= y1; ++y) {
            for (uint32_t x = x0; x <= x1; ++x) {
                Vec3f pixelSample(x + 0.5, y + 0.5, 0);
                float w0 = edgeFunction(v1Raster, v2Raster, pixelSample);
                float w1 = edgeFunction(v2Raster, v0Raster, pixelSample);
                float w2 = edgeFunction(v0Raster, v1Raster, pixelSample);
                if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                    w0 /= area;
                    w1 /= area;
                    w2 /= area;
                    float oneOverZ = v0Raster.z * w0 + v1Raster.z * w1 + v2Raster.z * w2;
                    float z = 1 / oneOverZ;
                    // [comment]
                    // Depth-buffer test
                    // [/comment]
                    if (z < depthBuffer[y * imageWidth + x]) {
                        depthBuffer[y * imageWidth + x] = z;
                        
                        // Vec2f st = st0 * w0 + st1 * w1 + st2 * w2;
                        
                        // st *= z;
                        
                        // [comment]
                        // If you need to compute the actual position of the shaded
                        // point in camera space. Proceed like with the other vertex attribute.
                        // Divide the point coordinates by the vertex z-coordinate then
                        // interpolate using barycentric coordinates and finally multiply
                        // by sample depth.
                        // [/comment]
                        Vec3f v0Cam, v1Cam, v2Cam;
                        worldToCamera.multVecMatrix(v0, v0Cam);
                        worldToCamera.multVecMatrix(v1, v1Cam);
                        worldToCamera.multVecMatrix(v2, v2Cam);
                        
                        float px = (v0Cam.x/-v0Cam.z) * w0 + (v1Cam.x/-v1Cam.z) * w1 + (v2Cam.x/-v2Cam.z) * w2;
                        float py = (v0Cam.y/-v0Cam.z) * w0 + (v1Cam.y/-v1Cam.z) * w1 + (v2Cam.y/-v2Cam.z) * w2;
                        
                        Vec3f pt(px * z, py * z, -z); // pt is in camera space
                        
                        // [comment]
                        // Compute the face normal which is used for a simple facing ratio.
                        // Keep in mind that we are doing all calculation in camera space.
                        // Thus the view direction can be computed as the point on the object
                        // in camera space minus Vec3f(0), the position of the camera in camera
                        // space.
                        // [/comment]
                        Vec3f n = (v1Cam - v0Cam).crossProduct(v2Cam - v0Cam);
                        n.normalize();
                        Vec3f viewDirection = -pt;
                        viewDirection.normalize();
                        
                        float nDotView =  std::max(0.f, n.dotProduct(viewDirection));
                        
                        int val = 255;
                        
                        if (nDotView  >= 0) {
                            ofs << "<line x1=\"" << v0Raster.x << "\" y1=\"" << v0Raster.y << "\" x2=\"" << v1Raster.x << "\" y2=\"" << v1Raster.y << "\" style=\"stroke:rgb(" << val << ",0,0);stroke-width:1\" />\n";
                            ofs << "<line x1=\"" << v1Raster.x << "\" y1=\"" << v1Raster.y << "\" x2=\"" << v2Raster.x << "\" y2=\"" << v2Raster.y << "\" style=\"stroke:rgb(" << val << ",0,0);stroke-width:1\" />\n";
                            ofs << "<line x1=\"" << v2Raster.x << "\" y1=\"" << v2Raster.y << "\" x2=\"" << v0Raster.x << "\" y2=\"" << v0Raster.y << "\" style=\"stroke:rgb(" << val << ",0,0);stroke-width:1\" />\n";
                        }
                    }
                }
            }
        }
    }
    
    ofs << "</svg>\n";
    ofs.close();
    
    delete [] frameBuffer;
    delete [] depthBuffer;
    
    return 0;
}
