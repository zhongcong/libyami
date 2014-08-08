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
const int kIPeriod = 30;
const int kDefaultFramerate = 30;
const int WIDTH = 192;
const int HEIGHT = 128;
const char* outFilename = "./test.264";

class StreamInput {
public:
    StreamInput();
    ~StreamInput();
    bool init(const char* fileName, const int width, const int height, const int frameNum);
    bool getOneFrameInput(VideoEncRawBuffer &inputBuffer);
    bool isFinish() {return m_readToFinish;};

private:
    FILE *m_fp;
    int m_width;
    int m_height;
    int m_frameNum;
    int m_frameSize;
    uint32_t m_lastReadOffset;
    uint8_t *m_buffer;

    bool m_readToFinish;
};

StreamInput::StreamInput()
    : m_fp(NULL)
    , m_width(0)
    , m_height(0)
    , m_frameNum(0)
    , m_frameSize(0)
    , m_lastReadOffset(0)
    , m_buffer(NULL)
    , m_readToFinish(false)
{
}

bool StreamInput::init(const char* fileName, const int width, const int height, const int frameNum)
{
    int32_t offset = -1;
    m_width = width;
    m_height = height;
    m_frameNum = frameNum;
    m_frameSize = m_width * m_height * 3 / 2;
    
    m_fp = fopen(fileName, "r");
    if (!m_fp) {
        fprintf(stderr, "fail to open input file: %s", fileName);
        return false;
    } else {
        printf("open input file : %s ok\n", fileName);
    }

    m_buffer = static_cast<uint8_t*>(malloc(m_frameSize));
    return true;
}

bool StreamInput::getOneFrameInput(VideoEncRawBuffer &inputBuffer)
{
    if (m_readToFinish)
        return false;

    if (fread(m_buffer, sizeof(uint8_t), m_frameSize, m_fp) != m_frameSize)
        return false;

    //parsing data for one frame
    inputBuffer.data = m_buffer;
    inputBuffer.size = m_frameSize;

//    DEBUG();
//    m_lastReadOffset += m_frameSize;
    return true;
}

StreamInput::~StreamInput()
{
    if(m_fp)
        fclose(m_fp);

    if(m_buffer)
        free(m_buffer);
}

bool writeOneOutputFrame(uint8_t* data, uint32_t dataSize)
{
    static FILE* fp = NULL;

    if(!fp) {
        fp = fopen(outFilename, "w+");
        if (!fp) {
            fprintf(stderr, "fail to open file: %s\n", outFilename);
            return false;
        }
    }

    printf("dataSize : %d\n", dataSize);
#if 1
    for (int i = 0; i < dataSize; i++) {
        if (!((i + 1) % 16))
            printf("\n");
        printf("\t%2x ", *(data + i));
    }
    printf("\n");
#endif

    if (fwrite(data, 1, dataSize, fp) != dataSize) {
        assert(0);
        return false;
    }

//    if(isOutputEOS)
//        fclose(fp);
    
    return true;
}

int main(int argc, char** argv)
{
    const char *fileName = NULL;
    StreamInput input;
    IVideoEncoder *encoder = NULL;
    VideoEncRawBuffer inputBuffer;
    VideoEncOutputBuffer outputBuffer;
    Display *x11Display = NULL;
    //VideoConfigBuffer configBuffer;
    Encode_Status status;
    Window window = 0;
    int32_t videoWidth = 0, videoHeight = 0;
    bool requestSPSPPS = true;

    if (argc < 2) {
        fprintf(stderr, "no input file to decode");
        return -1;
    }
    fileName = argv[1];
    INFO("yuv fileName: %s\n", fileName);

    if (!input.init(fileName, WIDTH, HEIGHT, 1)) {
        fprintf (stderr, "fail to init input stream\n");
        return -1;
    }

    x11Display = XOpenDisplay(NULL);
    encoder = createVideoEncoder("video/h264"); 
    encoder->setXDisplay(x11Display);

    //configure encoding parameters
    VideoParamsCommon encVideoParams;
//    encoder->getParameters(&encVideoParams);
    {
        //resolution
        encVideoParams.resolution.width = WIDTH;
        encVideoParams.resolution.height = HEIGHT;

        //frame rate parameters.
        encVideoParams.frameRate.frameRateDenom = 1;
        encVideoParams.frameRate.frameRateNum = kDefaultFramerate;

        //picture type and bitrate
        encVideoParams.intraPeriod = kIPeriod;

        encVideoParams.rcMode = RATE_CONTROL_CBR;
        encVideoParams.rcParams.bitRate = WIDTH * HEIGHT * kDefaultFramerate * 8;
        //encVideoParams.rcParams.initQP = 1;
        //encVideoParams.rcParams.minQP = 1;
        
        encVideoParams.profile = VAProfileH264Main;
        encVideoParams.rawFormat = RAW_FORMAT_YUV420;

        encVideoParams.level = 31;
    }
   printf("1\n"); 
    encoder->setParameters(&encVideoParams);
   printf("2\n"); 
    status = encoder->start(); 
#if 0
    while (!input.isEOS())
    {
        if (input.getOneFrameInput(inputBuffer))
            status = encoder->encode(&inputBuffer);
        else
            break;

        // render the frame if avaiable
        do {
           status = encoder->getOutput(&outputBuffer, false);
           //after getOutput buffer, we should write to file.
        } while (status != ENCODE_BUFFER_NO_MORE);
    }

    // drain the output buffer
    do {
       status = encoder->getOutput(&outputBuffer, false);
       //after getOutput buffer, we should write to file.
       output.write2File();
    } while (status != ENCODE_BUFFER_NO_MORE);
#endif

#if 1
   printf("3\n"); 
    input.getOneFrameInput(inputBuffer);
   printf("4\n"); 
    status = encoder->encode(&inputBuffer);

    //init output buffer
    outputBuffer.bufferSize = (WIDTH * HEIGHT * 3 / 2) * sizeof (uint8_t);
    outputBuffer.data = static_cast<uint8_t*>(malloc(outputBuffer.bufferSize));

    // output the frame if avaiable
    do {
        if (requestSPSPPS) {
            outputBuffer.format = OUTPUT_CODEC_DATA;
            requestSPSPPS = false;
        } else
            outputBuffer.format = OUTPUT_FRAME_DATA;

        status = encoder->getOutput(&outputBuffer, false);
        printf("status : %d\n", status);
        if (status == ENCODE_SUCCESS &&
            !writeOneOutputFrame(outputBuffer.data, outputBuffer.dataSize))
            assert(0);
       memset (outputBuffer.data, 0, outputBuffer.bufferSize);
    } while (status != ENCODE_BUFFER_NO_MORE);
#endif

    encoder->stop();
    releaseVideoEncoder(encoder);
}
