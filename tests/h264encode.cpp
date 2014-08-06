/*
 *  h264_encode.cpp - h264 encode test
 *
 *  Copyright (C) 2011-2014 Intel Corporation
 *    Author: Cong Zhong<congx.zhong@intel.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <X11/Xlib.h>

#include "common/log.h"
#include "VideoEncoderDef.h"
#include "VideoEncoderInterface.h"
#include "VideoEncoderHost.h"

using namespace YamiMediaCodec;
class StreamInput {
public:
    StreamInput();
    ~StreamInput();
    bool init(const char* fileName);
    bool getOneFrameInput(VideoEncodeBuffer &inputBuffer);
    bool isEOS() {return m_parseTOEOS;};

private:
    bool m_readToEOS;
    bool m_parseToEOS;
};

int main(int argc, char** argv)
{
    const char *fileName = NULL;
    StreamInput input;
    IVideoEncoder *encoder = NULL;
    VideoEncRawBuffer inputBuffer;
    VideoEncOutputBuffer outputBuffer;
    Display *x11Display = NULL;
    VideoConfigBuffer configBuffer;
    Encode_Status status;
    Window window = 0;
    int32_t videoWidth = 0, videoHeight = 0;

    if (argc < 2) {
        fprintf(stderr, "no input file to decode");
        return -1;
    }
    fileName = argv[1];
    INFO("yuv fileName: %s\n", fileName);

    if (!input.init(fileName)) {
        fprintf (stderr, "fail to init input stream\n");
        return -1;
    }

    x11Display = XOpenDisplay(NULL);
    encoder = createVideoEncoder("video/h264"); 
    encoder->setXDisplay(x11Display);

    //encoder->setParameters();//use default parameters
    //Note: do we need width/height
    status = encoder->start(); 

    while (!input.isEOS())
    {
        if (input.getOneFrameInput(inputBuffer)) {
            status = encoder->encode(&inputBuffer);
        else
            break;

        // render the frame if avaiable
        do {
           status = encoder->getOutput();
           //after getOutput buffer, we should write to file.
           output.write2File();
        } while (status != ENCODE_BUFFER_NO_MORE);
    }

    // drain the output buffer
    do {
       status = encoder->getOutput();
       //after getOutput buffer, we should write to file.
       output.write2File();
    } while (status != ENCODE_BUFFER_NO_MORE);

    encoder->stop();
    releaseVideoEncoder(encoder);
}
