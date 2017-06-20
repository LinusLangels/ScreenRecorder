//
// Created by Linus on 2017-03-02.
//

#ifndef ANDROIDNATIVECAPTURING_RGB2YUV420_H
#define ANDROIDNATIVECAPTURING_RGB2YUV420_H

#include <stddef.h>
#include <stdint.h>

void rgb2yuv420(uint8_t *destination, uint8_t *rgb, int bytesPerPixel, size_t width, size_t height)
{
    size_t image_size = width * height;
    size_t upos = image_size;
    size_t vpos = upos + upos / 4;
    size_t i = 0;

    for( size_t line = 0; line < height; ++line )
    {
        if( !(line % 2) )
        {
            for( size_t x = 0; x < width; x += 2 )
            {
                uint8_t r = rgb[bytesPerPixel * i + 0];
                uint8_t g = rgb[bytesPerPixel * i + 1];
                uint8_t b = rgb[bytesPerPixel * i + 2];

                destination[i++] = ((66*r + 129*g + 25*b) >> 8) + 16;

                destination[upos++] = ((-38*r + -74*g + 112*b) >> 8) + 128;
                destination[vpos++] = ((112*r + -94*g + -18*b) >> 8) + 128;

                r = rgb[bytesPerPixel * i + 0];
                g = rgb[bytesPerPixel * i + 1];
                b = rgb[bytesPerPixel * i + 2];

                destination[i++] = ((66*r + 129*g + 25*b) >> 8) + 16;
            }
        }
        else
        {
            for( size_t x = 0; x < width; x += 1 )
            {
                uint8_t r = rgb[bytesPerPixel * i + 0];
                uint8_t g = rgb[bytesPerPixel * i + 1];
                uint8_t b = rgb[bytesPerPixel * i + 2];

                destination[i++] = ((66*r + 129*g + 25*b) >> 8) + 16;
            }
        }
    }
}

#endif //ANDROIDNATIVECAPTURING_RGB2YUV420_H
