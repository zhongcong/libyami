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

class StreamInput {
public:
    StreamInput();
    ~StreamInput();
    bool init(const char* inputFileName, const int width, const int height);
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

bool StreamInput::init(const char* inputFileName, const int width, const int height)
{
    m_width = width;
    m_height = height;
    m_frameSize = m_width * m_height * 3 / 2;
    
    m_fp = fopen(inputFileName, "r");
    if (!m_fp) {
        fprintf(stderr, "fail to open input file: %s", inputFileName);
        return false;
    } else {
        printf("open input file : %s ok\n", inputFileName);
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
    bool init(const char* outputFileName, const int width, const int height);
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

bool StreamOutput::init(const char* outputFileName, const int width, const int height)
{
    m_width = width;
    m_height = height;
    m_frameSize = m_width * m_height * 3 / 2;
    
    m_fp = fopen(outputFileName, "w+");
    if (!m_fp) {
        fprintf(stderr, "fail to open output file: %s\n", outputFileName);
        return false;
    } else {
        printf("open output file : %s ok\n", outputFileName);
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
    char *inputFileName = NULL;
    char *outputFileName = NULL;    
    char *codec = NULL;
    char *colorspace = NULL;
    int videoWidth = 0, videoHeight = 0, bitRate = 0, fps = 0;

    StreamInput input;
    StreamOutput output;
    IVideoEncoder *encoder = NULL;
    VideoEncRawBuffer inputBuffer;
    Display *x11Display = NULL;
    Encode_Status status;
    bool requestSPSPPS = true;

    if (argc < 2) {
        fprintf(stderr, "can not encode without option\n");
        return -1;
    }

    //-------------------------------------------
    int opt;
    while ((opt = getopt(argc, argv, "W:H:b:f:c:s:i:o:h")) != -1)
    {
        switch (opt) {
        case 'i':
            inputFileName = optarg;
            printf("inputFileName : %s\n", inputFileName);
            break;
        case 'o':
            outputFileName = optarg;
            printf("outputFileName : %s\n", outputFileName);
            break;
        case 'W':
            videoWidth = atoi(optarg);
            printf("width : %d\n", videoWidth);
            break;
        case 'H':
            videoHeight = atoi(optarg);
            printf("height : %d\n", videoHeight);
            break;
        case 'b':
            bitRate = atoi(optarg);
            printf("bitRate : %d\n", bitRate);
            break;
        case 'f':
            fps = atoi(optarg);
            printf("fps : %d\n", fps);
            break;
        case 'c':
            codec = optarg;
            printf ("codec : %s\n", codec);
            break;
        case 's':
            colorspace = optarg;
            printf("colorspace : %s\n", colorspace);
            break;
        case 'h':
            break;
        case '?':
            printf("Unknown option: %c\n", (char)optopt);
            break;
        }
    }

    //-------------------------------------------
    if (!inputFileName) {
        fprintf(stderr, "can not encode without input file\n");
        return -1;
    } else
        fprintf(stdout, "yuv fileName: %s\n", inputFileName);

    if (!videoWidth || !videoHeight) {
        fprintf(stderr, "can not encode without video/height\n");
        return -1;
    }

    if (!fps)
        fps = 30;

    if (!bitRate)
        bitRate = videoWidth * videoHeight * fps  * 8;
        
    if (!outputFileName)
        outputFileName = "test.yuv";

    if (!input.init(inputFileName, videoWidth, videoHeight)) {
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
        encVideoParams.resolution.width = videoWidth;
        encVideoParams.resolution.height = videoHeight;

        //frame rate parameters.
        encVideoParams.frameRate.frameRateDenom = 1;
        encVideoParams.frameRate.frameRateNum = fps;

        //picture type and bitrate
        encVideoParams.intraPeriod = kIPeriod;
        encVideoParams.rcMode = RATE_CONTROL_CBR;
        //encVideoParams.rcParams.bitRate = videoWidth * videoHeight * kDefaultFramerate * 3 / 2 * 8;//why this is wrong
        encVideoParams.rcParams.bitRate = bitRate;
        //encVideoParams.rcParams.initQP = 26;
        //encVideoParams.rcParams.minQP = 1;
        
        encVideoParams.profile = VAProfileH264Main;
        encVideoParams.rawFormat = RAW_FORMAT_YUV420;

        encVideoParams.level = 31;
    }

    encoder->setParameters(&encVideoParams);
    status = encoder->start();

    //init output buffer
    if (!output.init(outputFileName, videoWidth, videoHeight)) {
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
