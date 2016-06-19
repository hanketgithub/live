/**********
 This library is free software; you can redistribute it and/or modify it under
 the terms of the GNU Lesser General Public License as published by the
 Free Software Foundation; either version 2.1 of the License, or (at your
 option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)
 
 This library is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 more details.
 
 You should have received a copy of the GNU Lesser General Public License
 along with this library; if not, write to the Free Software Foundation, Inc.,
 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 **********/
// Copyright (c) 1996-2015, Live Networks, Inc.  All rights reserved
// A test program that reads a H.265 Elementary Stream video file
// and streams it using RTP
// main program
//
// NOTE: For this application to work, the H.265 Elementary Stream video file *must* contain
// VPS, SPS and PPS NAL units, ideally at or near the start of the file.
// These VPS, SPS and PPS NAL units are used to specify 'configuration' information that is set in
// the output stream's SDP description (by the RTSP server that is built in to this application).
// Note also that - unlike some other "*Streamer" demo applications - the resulting stream can be
// received only using a RTSP client (such as "openRTSP")

#include <stdint.h>
#include <string>
#include <fstream>
#include <iostream>

#include <libvega_encoder_api/VEGA330X_types.h>
#include <libvega_encoder_api/VEGA330X_encoder.h>
#include <HvcEncoder.hh>

#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>


using namespace std;

UsageEnvironment* env;
char const * inputFileName;
H265VideoStreamFramer* videoSource;
RTPSink* videoSink;

extern Encoder *pstEncoder;

void play(); // forward

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        fprintf(stderr, "useage: %s [input_file] [width] [height]\n", argv[0]);
        
        return -1;
    }

    inputFileName = argv[1];

    // Begin by setting up our usage environment:
    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    env = BasicUsageEnvironment::createNew(*scheduler);
    
    // Create 'groupsocks' for RTP and RTCP:
    struct in_addr destinationAddress;
    destinationAddress.s_addr = chooseRandomIPv4SSMAddress(*env);
    // Note: This is a multicast address.  If you wish instead to stream
    // using unicast, then you should use the "testOnDemandRTSPServer"
    // test program - not this test program - as a model.
    
    const unsigned short rtpPortNum = 18888;
    const unsigned short rtcpPortNum = rtpPortNum+1;
    const unsigned char ttl = 255;
    
    const Port rtpPort(rtpPortNum);
    const Port rtcpPort(rtcpPortNum);
    
    Groupsock rtpGroupsock(*env, destinationAddress, rtpPort, ttl);
    rtpGroupsock.multicastSendOnly(); // we're a SSM source
    Groupsock rtcpGroupsock(*env, destinationAddress, rtcpPort, ttl);
    rtcpGroupsock.multicastSendOnly(); // we're a SSM source
    
    // Create a 'H265 Video RTP' sink from the RTP 'groupsock':
    OutPacketBuffer::maxSize = 100000;
    videoSink = H265VideoRTPSink::createNew(*env, &rtpGroupsock, 96);
    
    // Create (and start) a 'RTCP instance' for this RTP sink:
    const unsigned estimatedSessionBandwidth = 500; // in kbps; for RTCP b/w share
    const unsigned maxCNAMElen = 100;
    unsigned char CNAME[maxCNAMElen+1];
    gethostname((char *) CNAME, maxCNAMElen);
    CNAME[maxCNAMElen] = '\0'; // just in case
    RTCPInstance* rtcp
    = RTCPInstance::createNew(*env, &rtcpGroupsock,
                              estimatedSessionBandwidth, CNAME,
                              videoSink, NULL /* we're a server */,
                              True /* we're a SSM source */);
    // Note: This starts RTCP running automatically
    
    RTSPServer* rtspServer = RTSPServer::createNew(*env, 8554);
    if (rtspServer == NULL) {
        *env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
        exit(1);
    }
    ServerMediaSession* sms
    = ServerMediaSession::createNew(*env, "testStream", inputFileName,
                                    "Session streamed by \"testH265VideoStreamer\"",
                                    True /*SSM*/);
    sms->addSubsession(PassiveServerMediaSubsession::createNew(*videoSink, rtcp));
    rtspServer->addServerMediaSession(sms);
    
    char* url = rtspServer->rtspURL(sms);
    *env << "Play this stream using the URL \"" << url << "\"\n";
    delete[] url;

    ifstream inputFile;

    inputFile.open(inputFileName, ios::in | ios::binary);

    if (!inputFile)
    {
        fprintf(stderr, "Can not open %s!\n", inputFileName);
        return -1;
    }

    int width = atoi(argv[2]);
    int height = atoi(argv[3]);
    int wxh = width * height;

    cout << "W=" << width << ", H=" << height << endl;

    API_HVC_BOARD_E eBoard = API_HVC_BOARD_1;
    API_HVC_CHN_E eCh = API_HVC_CHN_1;

    pstEncoder = new Encoder(inputFile, wxh * 3 / 2, eBoard, eCh);

    pstEncoder->setInputMode(API_HVC_INPUT_MODE_DATA);
    pstEncoder->setProfile(API_HVC_HEVC_MAIN_PROFILE);
    pstEncoder->setLevel(API_HVC_HEVC_LEVEL_40);
    pstEncoder->setTier(API_HVC_HEVC_MAIN_TIER);
    pstEncoder->setResolution(API_HVC_RESOLUTION_720x576);
    pstEncoder->setChromaFormat(API_HVC_CHROMA_FORMAT_420);
    pstEncoder->setBitDepth(API_HVC_BIT_DEPTH_8);
    pstEncoder->setGopType(API_HVC_GOP_IB);
    pstEncoder->setGopSize(API_HVC_GOP_SIZE_64);
    pstEncoder->setBnum(API_HVC_B_FRAME_MAX);
    pstEncoder->setFps(API_HVC_FPS_29_97);
    pstEncoder->setBitrate(1000);
    

    if (!pstEncoder->init())
    {
        return 0;
    }

    if (!pstEncoder->start())
    {
        return 0;
    }

    /* Push 9 images */
    for (int i = 0; i < 9; i++)
    {
        API_VEGA330X_IMG_T img;
        uint8_t *blank;

        memset(&img, 0, sizeof(img));
        blank = (uint8_t *) calloc(wxh * 3 / 2, sizeof(uint8_t));

        img.pu8Addr = blank;
        img.u32Size = wxh * 3 / 2;
        img.bLastFrame = false;
        VEGA330X_ENC_PushImage(eBoard, eCh, &img);
        free(blank);
    }

    // Start the streaming:
    *env << "Beginning streaming...\n";
    play();
    
    env->taskScheduler().doEventLoop(); // does not return

    return 0; // only to prevent compiler warning
}

void afterPlaying(void* /*clientData*/)
{
    *env << "...done reading from file\n";
    videoSink->stopPlaying();
    Medium::close(videoSource);
    // Note that this also closes the input file that this source read from.
    
    // Start playing once again:
    play();
}

void play()
{
    // Open the input file as a 'byte-stream file source':
    ByteStreamFileSource* fileSource = ByteStreamFileSource::createNew(*env, inputFileName);

    if (fileSource == NULL)
    {
        *env << "Unable to open file \"" << inputFileName
        << "\" as a byte-stream file source\n";
        exit(1);
    }
    
    FramedSource* videoES = fileSource;
    
    // Create a framer for the Video Elementary Stream:
    videoSource = H265VideoStreamFramer::createNew(*env, videoES);
    
    // Finally, start playing:
    *env << "Beginning to read from file...\n";
    videoSink->startPlaying(*videoSource, afterPlaying, videoSink);
}

