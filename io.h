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

typedef struct Buffer {
    void *begin;
    size_t len;
} Buffer;

int openCard(char *d);
int getVInputs(int fd, struct v4l2_input *vInputs);
int getFormats(int fd, struct v4l2_fmtdesc *formats, int *bgr32);
void getCropCap(int fd, struct v4l2_cropcap *cropcap);
int getSizes(int fd, struct v4l2_frmsizeenum *sizes, __u32 format);
void setSize(int fd, struct v4l2_fmtdesc *format,
    struct v4l2_frmsizeenum *size);
void initializeStreaming(int fd, Buffer *buffer, struct v4l2_buffer *vbuffer);
void endStreaming(int fd, Buffer *buffer);
int getFrame(int fd, struct v4l2_buffer *vbuffer);
