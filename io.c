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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "io.h"

int openCard(char *d){
    char device[81];
    int i, r;
    char *a, *b;

    /* Try to open the device the user specified */
    if(d[0]){
        return open(d, O_RDWR | O_NONBLOCK, 0);
    }

    /* Try every possible /dev/video */
    sprintf(device, "/dev/video");
    r = open(device, O_RDWR | O_NONBLOCK, 0);
    for(i = 0; i < 64 && r < 0; i++){
        sprintf(device, "/dev/video%d", i);
        r = open(device, O_RDWR | O_NONBLOCK, 0);
    }

    /* Store the opened the device in d */
    a = d;
    b = device;
    do {
        *(a++) = *b;
    } while(*(b++));

    return r;
}

int getVInputs(int fd, struct v4l2_input *vInputs){
    int i;

    for(i = 0; i < 128; i++){
        vInputs[i].index = i;
        if(ioctl(fd, VIDIOC_ENUMINPUT, &(vInputs[i])) < 0){
            break;
        }
    }

    return i;
}

int getFormats(int fd, struct v4l2_fmtdesc *formats, int *bgr32){
    int i;

    for(i = 0; i < 128; i++){
        formats[i].index = i;
        formats[i].type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if(ioctl(fd, VIDIOC_ENUM_FMT, &(formats[i])) < 0){
            break;
        }
        /* Save the bgr32 format index */
        if(formats[i].pixelformat == V4L2_PIX_FMT_BGR32){
            *bgr32 = i;
        }
    }

    return i;
}

void getCropCap(int fd, struct v4l2_cropcap *cropcap){
    cropcap->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(fd, VIDIOC_CROPCAP, cropcap);
}

int getSizes(int fd, struct v4l2_frmsizeenum *sizes, __u32 format){
    int i;

    for(i = 0; i < 128; i++){
        sizes[i].index = i;
        sizes[i].pixel_format = format;
        if(ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &(sizes[i])) < 0){
            break;
        }
    }

    return i;
}

void setSize(int fd, struct v4l2_fmtdesc *format,
    struct v4l2_frmsizeenum *size){

    struct v4l2_format f;

    memset(&f, 0, sizeof(f));

    f.type = format->type;

    if(size->type == V4L2_FRMSIZE_TYPE_DISCRETE){
        f.fmt.pix.width = size->discrete.width;
        f.fmt.pix.height = size->discrete.height;
    }
    else {
        f.fmt.pix.width = size->stepwise.step_width;
        f.fmt.pix.height = size->stepwise.step_height;
    }

    f.fmt.pix.pixelformat = format->pixelformat;
    f.fmt.pix.field = V4L2_FIELD_ANY;
    f.fmt.pix.bytesperline = 0;

    ioctl(fd, VIDIOC_S_FMT, &f);
}

void initializeStreaming(int fd, Buffer *buffer, struct v4l2_buffer *vbuffer){
    struct v4l2_requestbuffers reqbuf;

    /* Get only one buffer */
    memset(&reqbuf, 0, sizeof(reqbuf));
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    reqbuf.count = 1;
    ioctl(fd, VIDIOC_REQBUFS, &reqbuf);

    memset(vbuffer, 0, sizeof(struct v4l2_buffer));

    vbuffer->type = reqbuf.type;
    vbuffer->memory = V4L2_MEMORY_MMAP;
    vbuffer->index = 0;

    ioctl(fd, VIDIOC_QUERYBUF, vbuffer);

    buffer->len = vbuffer->length;
    buffer->begin = mmap(NULL, vbuffer->length,
        PROT_READ, MAP_SHARED,
        fd, vbuffer->m.offset);

    /* Start streaming */
    vbuffer->index = 0;
    vbuffer->bytesused = 0;
    memset(&vbuffer->timestamp, 0, sizeof(vbuffer->timestamp));
    ioctl(fd, VIDIOC_QBUF, vbuffer);
    ioctl(fd, VIDIOC_STREAMON, NULL);
}

int getFrame(int fd, struct v4l2_buffer *vbuffer){
    fd_set set;

    vbuffer->index = 0;
    vbuffer->bytesused = 0;
    if(ioctl(fd, VIDIOC_QBUF, vbuffer)){
        return 1;
    }
    FD_SET(fd, &set);
    select(FD_SETSIZE, &set, NULL, NULL, NULL);
    if(ioctl(fd, VIDIOC_DQBUF, vbuffer)){
        return 1;
    }

    return 0;
}

void endStreaming(int fd, Buffer *buffer){
    ioctl(fd, VIDIOC_STREAMOFF, NULL);
    munmap(buffer->begin, buffer->len);
}
