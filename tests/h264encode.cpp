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
#include <unistd.h>
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
    bool init(const char* fileName, const int width, const int height);
    bool getOneFrameInput(VideoEncRawBuffer &inputBuffer);
    bool isEOS() {return m_readToEOS;};

private:
    FILE *m_fp;
    int m_width;
    int m_height;
    int m_frameSize;
    uint8_t *m_buffer;

    bool m_readToEOS;
};

StreamInput::StreamInput()
    : m_fp(NULL)
    , m_width(0)
    , m_height(0)
    , m_frameSize(0)
    , m_buffer(NULL)
    , m_readToEOS(false)
{
}

bool StreamInput::init(const char* fileName, const int width, const int height)
{
    m_width = width;
    m_height = height;
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
    if (m_readToEOS)
        return false;

    static int num = 0;
    
    int ret = fread(m_buffer, sizeof(uint8_t), m_frameSize, m_fp);

    if (ret <= 0) {
        m_readToEOS = true;
        return false;
    } else if (ret < m_frameSize) {
        printf ("data is not enough to read, maybe resolution is wrong\n");
        return false;
    } else {
        printf ("frame num : %d\n", ++num);
        inputBuffer.data = m_buffer;
        inputBuffer.size = m_frameSize;
    }

    return true;
}

StreamInput::~StreamInput()
{
    if(m_fp)
        fclose(m_fp);

    if(m_buffer)
        free(m_buffer);
}

class StreamOutput {
public:    
    StreamOutput();
    ~StreamOutput();
    bool init(const int width, const int height);
    bool writeOneOutputFrame();
    void resetBuffer();
    VideoEncOutputBuffer outputBuffer;

private:
    FILE *m_fp;
    int m_width;
    int m_height;
    int m_frameNum;
    int m_frameSize;
    uint8_t *m_buffer;    
};

StreamOutput::StreamOutput()
    : m_fp(NULL)
    , m_width(0)
    , m_height(0)
    , m_frameNum(0)
    , m_frameSize(0)
    , m_buffer(NULL)
{
}

bool StreamOutput::init(const int width, const int height)
{
    m_width = width;
    m_height = height;
    m_frameSize = m_width * m_height * 3 / 2;
    
    m_fp = fopen(outFilename, "w+");
    if (!m_fp) {
        fprintf(stderr, "fail to open output file: %s\n", outFilename);
        return false;
    } else {
        printf("open output file : %s ok\n", outFilename);
    }

    m_buffer = static_cast<uint8_t*>(malloc(m_frameSize));
    outputBuffer.bufferSize = m_frameSize;
    outputBuffer.data = m_buffer;

    return true;
}

bool StreamOutput::writeOneOutputFrame()
{
    printf("dataSize : %d\n", outputBuffer.dataSize);
#if 0
    for (int i = 0; i < dataSize; i++) {
        if (!((i + 1) % 16))
            printf("\n");
        printf("\t%2x ", *(data + i));
    }
    printf("\n");
#endif

    if (fwrite(m_buffer, 1, outputBuffer.dataSize, m_fp) != outputBuffer.dataSize) {
        assert(0);
        return false;
    }

    resetBuffer();
    return true;
}

void StreamOutput::resetBuffer()
{
    memset (outputBuffer.data, 0, outputBuffer.bufferSize);
}

StreamOutput::~StreamOutput()
{
    if(m_fp)
        fclose(m_fp);

    if(m_buffer)
        free(m_buffer);
}

int main(int argc, char** argv)
{
    char inputFileName[16] = {0};
    char outputFileName[16] = {0};
    StreamInput input;
    StreamOutput output;
    IVideoEncoder *encoder = NULL;
    VideoEncRawBuffer inputBuffer;
    Display *x11Display = NULL;
    Encode_Status status;
    int32_t videoWidth = 0, videoHeight = 0;
    bool requestSPSPPS = true;

    if (argc < 2) {
        fprintf(stderr, "no input file to decode");
        return -1;
    }

    //-------------------------------------------
    int opt;
    while ((opt = getopt(argc, argv, "W:H:b:f:c:s:i:o:h")) != -1)
    {
        switch (opt) {
        case 'i':
            memcpy(inputFileName, optarg, 16);
            printf("inputFileName : %s\n", inputFileName);
            break;
        case 'o':
            memcpy(outputFileName, optarg, 16);
            printf("outputFileName : %s\n", outputFileName);
            break;
        case 'W':
            char width[8];
            memcpy(width, optarg, 8);
            printf("width : %s\n", width);
            break;
        case 'H':
            char height[8];
            memcpy(height, optarg, 8);
            printf("height : %s\n", height);
            break;
        case 'b':
            char bitRate[16];
            memcpy(bitRate, optarg, 16);
            break;
        case 'f':
            char fps[16];
            memcpy(fps, optarg, 16);
            break;
        case 'c':
            char codec[8];
            memcpy(codec, optarg, 8);
            break;
        case 's':
            char colorspace[8];
            memcpy(codec, optarg, 8);
            break;
        case 'h':
            break;
        case '?':
            printf("Unknown option: %c\n", (char)optopt);
            break;
        }
    }

    //-------------------------------------------
    INFO("yuv fileName: %s\n", inputFileName);

    if (!input.init(inputFileName, WIDTH, HEIGHT)) {
        fprintf (stderr, "fail to init input stream\n");
        return -1;
    }

    x11Display = XOpenDisplay(NULL);
    encoder = createVideoEncoder("video/h264"); 
    encoder->setXDisplay(x11Display);

    //configure encoding parameters
    VideoParamsCommon encVideoParams;
    encoder->getParameters(&encVideoParams);
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

    encoder->setParameters(&encVideoParams);
    status = encoder->start();

    //init output buffer
    if (!output.init(WIDTH, HEIGHT)) {
        fprintf (stderr, "fail to init input stream\n");
        return -1;
    }

    while (!input.isEOS())
    {
        if (input.getOneFrameInput(inputBuffer))
            status = encoder->encode(&inputBuffer);
        else
            break;

        //get the output buffer
        do {
            if (requestSPSPPS) {
                output.outputBuffer.format = OUTPUT_CODEC_DATA;
                requestSPSPPS = false;
            } else
                output.outputBuffer.format = OUTPUT_FRAME_DATA;

            status = encoder->getOutput(&output.outputBuffer, false);
            printf("status : %d\n", status);
            if (status == ENCODE_SUCCESS && !output.writeOneOutputFrame())
                assert(0);
        } while (status != ENCODE_BUFFER_NO_MORE);
    }

    // drain the output buffer
    do {
       status = encoder->getOutput(&output.outputBuffer, false);
       printf("status : %d\n", status);
       if (status == ENCODE_SUCCESS && !output.writeOneOutputFrame())
           assert(0);
    } while (status != ENCODE_BUFFER_NO_MORE);

    encoder->stop();
    releaseVideoEncoder(encoder);
}
