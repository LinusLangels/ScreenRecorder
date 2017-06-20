//
// Created by Linus on 2017-02-23.
//

#ifndef ANDROIDNATIVECAPTURING_DEBUGTOOLS_H
#define ANDROIDNATIVECAPTURING_DEBUGTOOLS_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static void create_binary_rgb(const char* debugPath, char *prefix, int frame_id, int width, int height, int pixelSize, unsigned char *pixel)
{
    enum Constants { max_filename = 256 };
    char filename[max_filename];
    snprintf(filename, max_filename, "%s/%s%d.raw", debugPath, prefix, frame_id);
    FILE *f = fopen(filename, "w");

    fwrite(pixel, width*height*pixelSize, 1, f);
    fclose(f);
}

#endif //ANDROIDNATIVECAPTURING_DEBUGTOOLS_H
