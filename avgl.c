/*
 * avgl - A very simple TV viewer for Linux using v4l, SDL and OpenGL
 * avgl Copyright (C) 2011, 2012 Flávio Zavan
 *
 * This file is part of avgl
 *
 * avgl is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * avgl is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NebSweeper Launcher.  If not, see <http://www.gnu.org/licenses/>.
 *
 * flavio [AT] nebososo [DOT] com
 * http://www.nebososo.com
*/

#include <linux/videodev2.h>
#include <libv4l1-videodev.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <SDL/SDL.h>
#include <SDL/SDL_opengl.h>
#include "io.h"

int main(int argc, char **argv){
    int fs, width, height, verbose, query, resize, scale, contrast;
    char c;
    char videoDevice[81];
    int fd, i, nInputs, vInput, nFormats, nSizes, size, format;
    v4l2_std_id newStd;
    struct v4l2_input vInputs[128];
    struct v4l2_capability cap;
    struct v4l2_fmtdesc formats[128];
    struct v4l2_frmsizeenum sizes[128];
    struct v4l2_buffer vbuffer;
    Buffer buffer;
    SDL_Event event;
    SDL_Surface *screen;
    GLuint texture;
    int linear;
    float minIntensity, maxIntensity, intensityMultiplier;
    uint8_t *buf;
    int p, M, m;
    int histogram[256];

    char standards[26][12] = {"PAL-B", "PAL-B1", "PAL-G", "PAL-H", "PAL-I",
        "PAL-D", "PAL-D1", "PAL-K", "PAL-M", "PAL-N", "PAL-Nc", "PAL-60",
        "NTSC-M", "NTSC-M-JP", "NTSC-443", "NTSC-M-KR", "SECAM-B", "SECAM-D",
        "SECAM-G", "SECAM-H", "SECAM-K", "SECAM-L", "SECAM-LC", "ATSC-8-VSB",
        "ATSC-16-VSB"};

    fs = 0;
    width = 0;
    height = 0;
    vInput = 0;
    newStd = V4L2_STD_NTSC_M;
    videoDevice[0] = '\0';
    verbose = 0;
    format = -1;
    query = 0;
    size = 0;
    linear = 1;
    scale = 1;
    contrast = 0;
    resize = SDL_RESIZABLE;
    minIntensity = 0;
    maxIntensity = 0;
    intensityMultiplier = 1.0;

    /* Parse the command line */
    while((c = getopt(argc, argv, "fD:hw:H:i:s:QvlrS:c")) >= 0){
        switch(c){
            case 'f':
                fs = 1;
                break;

            case 'D':
                sscanf(optarg, "%80s", videoDevice);
                break;

            case 'w':
                width = atoi(optarg);
                break;

            case 'H':
                height = atoi(optarg);
                break;

            case 'i':
                vInput = atoi(optarg);
                break;

            case 's':
                sscanf(optarg, "%lx", (long unsigned int *) &newStd);
                break;

            case 'v':
                verbose = 1;
                break;

            case 'Q':
                query = 1;
                verbose = 1;
                break;

            case 'l':
                linear = 1;
                break;

            case 'r':
                resize = 0;
                break;

            case 'S':
                sscanf(optarg, "%d", &scale);
                break;

            case 'c':
                contrast = 1;
                break;

            case 'h':
            case '?':
                fprintf(stderr, "Usage: %s [OPTION]...\n\n"
                    "    -D    specify the video device to use "
                    "(i.e. /dev/video0)\n"
                    "    -c    auto contrast correction\n"
                    "    -f    fullscreen mode\n"
                    "    -h    show this message\n"
                    "    -H    specify the video height\n"
                    "    -i    specify the video input (i.e. 0)\n"
                    "    -l    use bilinear interpolation\n"
                    "    -Q    query inputs, standards, formats and sizes\n"
                    "    -r    disable window resize\n"
                    "    -s    specify the standard (i.e. 0x1000 for NTSC)\n"
                    "    -S    initial window scaling (must be an integer)\n"
                    "    -v    verbose\n"
                    "    -w    specify video width\n",
                    argv[0]);

                return 0;
        }
    }

    /* Open the card manually to set other options */
    fd = openCard(videoDevice);

    if(fd < 0){
        fprintf(stderr, "Failed to open the video device.\n"
            "Is it connected?\n"
            "You can specify it using the -D flag.\n");

        return 1;
    }
    else {
        fprintf(stderr, "Opened %s.\n\n", videoDevice);
    }

    /* Query capabilities and check if it's valid */
    if(ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0){
        fprintf(stderr, "%s is not a valid V4L2 device.\n", videoDevice);
        close(fd);

        return 1;
    }
    else if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)){
        fprintf(stderr, "%s cannot capture videos\n", videoDevice);
        close(fd);

        return 1;
    }

    /* Select the input and format we want */
    if(ioctl(fd, VIDIOC_S_INPUT, &(vInputs[vInput].index)) < 0){
        fprintf(stderr, "Failed to set the input to %d.\n", vInput);
    }
    if(ioctl(fd, VIDIOC_S_STD, &newStd) < 0){
        fprintf(stderr, "Failed to set the standard to %lX.\n",
            (unsigned long int) newStd);
    }

    /* Query inputs, formats and sizes */
    nInputs = getVInputs(fd, vInputs);
    nFormats = getFormats(fd, formats, &format);
    nSizes = getSizes(fd, sizes, V4L2_PIX_FMT_BGR32);
    if(format < 0){
        fprintf(stderr, "%s does not support BGR32\n", videoDevice);
        close(fd);
        exit(1);
    }

    if(verbose){
        /* Print them */
        fprintf(stderr, "List of inputs:\n");
        for(i = 0; i < nInputs; i++){
            fprintf(stderr, "    %d: %s\n", i, vInputs[i].name);
        }
        fprintf(stderr, "\n");

        fprintf(stderr, "List of formats:\n");
        for(i = 0; i < nFormats; i++){
            fprintf(stderr, "    %d: %s\n", i, formats[i].description);
        }
        fprintf(stderr, "    Using BGR32, index %d\n\n", format);

        fprintf(stderr, "List of sizes:\n");
        for(i = 0; i < nSizes; i++){
            if(sizes[i].type == V4L2_FRMSIZE_TYPE_DISCRETE){
                fprintf(stderr, "    %d: %dx%d\n",
                    i, sizes[i].discrete.width, sizes[i].discrete.height);
            }
            else {
                fprintf(stderr, "    %d: %dx%d\n",
                    i,
                    sizes[i].stepwise.step_width,
                    sizes[i].stepwise.step_height);
            }
        }
        fprintf(stderr, "\n");

        /* List the available standards for the selected input */
        fprintf(stderr, "Available standards:\n");
        for(i = 0; i < 26; i++){
            if(vInputs[0].std & 1 << i){
                fprintf(stderr, "    %s: 0x%X\n",
                    standards[i], 1 << i);
            }
        }
        fprintf(stderr, "\n");
    }

    if(query){
        close(fd);

        return 0;
    }

    /* Try to set the selected size */
    if(width || height){
        size = -1;

        for(i = 0; i < nSizes; i++){
            if(sizes[i].type == V4L2_FRMSIZE_TYPE_DISCRETE){
                    if(sizes[i].discrete.width == width &&
                        sizes[i].discrete.height == height){

                        size = i;
                        break;
                    }
            }
            else {
                if(sizes[i].stepwise.step_width == width &&
                    sizes[i].stepwise.step_height == height){

                    size = i;
                    break;
                }
            }
        }
    }

    /* If it exists, then set it */
    if(size >= 0){
        if(sizes[size].type == V4L2_FRMSIZE_TYPE_DISCRETE){
            width = sizes[size].discrete.width;
            height = sizes[size].discrete.height;
        }
        else {
            width = sizes[size].stepwise.step_width;
            height = sizes[size].stepwise.step_height;
        }

        if(verbose){
            fprintf(stderr, "Using size %dx%d\n\n", width, height);
        }
    }
    /* Pick a valid one if it's invalid */
    else {
        size = 0;

        fprintf(stderr, "%dx%d is not a supported size.\n",
            width, height);

        if(sizes[0].type == V4L2_FRMSIZE_TYPE_DISCRETE){
            width = sizes[0].discrete.width;
            height = sizes[0].discrete.height;
        }
        else {
            width = sizes[0].stepwise.step_width;
            height = sizes[0].stepwise.step_height;
        }

        fprintf(stderr, "Using %dx%d.\n"
            "Run with -Q to get a list of supported sizes.\n",
            width, height);
    }

    /* Actually set the size */
    setSize(fd, formats + format, sizes + size);
    
    /* Request a buffer for streaming */
    initializeStreaming(fd, &buffer, &vbuffer);

    /* Initialize SDL */
    if(SDL_Init(SDL_INIT_VIDEO)){
        fprintf(stderr, "Unable to initialize SDL.\n");
        goto cleanup;
    }


    /* Initialize the screen to use BGR32 */
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    if(!(screen = SDL_SetVideoMode((fs? 0 : width * scale),
                (fs? 0 : height * scale), 32,
                resize | SDL_OPENGL | (fs? SDL_FULLSCREEN : 0)))){

        SDL_Quit();
        fprintf(stderr, "Could not create screen.\n");
        goto cleanup;
    }
    SDL_WM_SetCaption("avgl", NULL);

    /* Set up OpenGL for 2D */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, width, height, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glViewport(0, 0, screen->w, screen->h);

    /* Generate the video texture */
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
        (linear? GL_LINEAR : GL_NEAREST));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
        (linear? GL_LINEAR : GL_NEAREST));

    event.type = 0;
    buf = (uint8_t *) buffer.begin;
    getFrame(fd, &vbuffer);
    do {
        /* Render the current frame */
        glEnable(GL_TEXTURE_2D);
        glTexImage2D(GL_TEXTURE_2D, 0, 4, width, height, 0,
            GL_BGRA, GL_UNSIGNED_BYTE, buffer.begin);
        glBindTexture(GL_TEXTURE_2D, texture);
        glBegin(GL_QUADS);
        glTexCoord2i(0, 0);
        glVertex2f(0, 0);
        glTexCoord2i(1, 0);
        glVertex2f(width, 0);
        glTexCoord2i(1, 1);
        glVertex2f(width, height);
        glTexCoord2i(0, 1);
        glVertex2f(0, height);
        glEnd();
        glDisable(GL_TEXTURE_2D);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
        glColor4f(1.0, 1.0, 1.0, minIntensity);
        glBegin(GL_QUADS);
        glVertex2f(0, 0);
        glVertex2f(width, 0);
        glVertex2f(width, height);
        glVertex2f(0, height);
        glEnd();

        glBlendFunc(GL_ZERO, GL_SRC_ALPHA);
        glColor4f(1.0, 1.0, 1.0, intensityMultiplier);
        glBegin(GL_QUADS);
        glVertex2f(0, 0);
        glVertex2f(width, 0);
        glVertex2f(width, height);
        glVertex2f(0, height);
        glEnd();
        glDisable(GL_BLEND);

        SDL_GL_SwapBuffers();

        /* Process Input */
        SDL_PollEvent(&event);

        if(event.type == SDL_KEYDOWN){
            if(event.key.keysym.sym == SDLK_ESCAPE){
                break;
            }
        }
        else if(!fs && event.type == SDL_VIDEORESIZE){
            SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
            screen = SDL_SetVideoMode(event.resize.w, event.resize.h, 32,
                resize | SDL_OPENGL | (fs? SDL_FULLSCREEN : 0));

            glViewport(0, 0, screen->w, screen->h);
        }

        if(getFrame(fd, &vbuffer)){
            fprintf(stderr, "Can't get frames.\n");
            break;
        }

        if(contrast){
            m = 0x3f3f3f3f;
            M = -0x3f3f3f3f;
            memset(histogram, 0, sizeof(histogram));
            for(i = 0; i < width * height * 4; i += 4){
                p = (buf[i] + buf[i + 1] + buf[i + 2]) / 3;
                histogram[p]++;
            }

            m = 0;
            for(i = 1; i < 255; i++){
                if(histogram[i - 1] < histogram[i] &&
                    histogram[i + 1] < histogram[i]){

                    m = i;
                    break;
                }
            }
            M = 255;
            for(i = 254; i > 0; i--){
                if(histogram[i - 1] < histogram[i] &&
                    histogram[i + 1] < histogram[i]){

                    M = i;
                    break;
                }
            }

            minIntensity = (1.0 / 256.0) * (float) m;
            maxIntensity = (1.0 / 256.0) * (float) M;
            intensityMultiplier = (1.0 - minIntensity) /
                (1.0 - minIntensity - maxIntensity);
        }
    } while(event.type != SDL_QUIT);

    SDL_Quit();
    cleanup:
    endStreaming(fd, &buffer);
    close(fd);

    return 0;
}
