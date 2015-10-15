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

#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#include "m31_hvc_api/HVC_types.h"
#include "m31_hvc_api/HVC_encoder.h"


typedef struct
{
    API_HVC_BOARD_E eBoard;
    API_HVC_CHN_E   eCh;
    int        		fd_vraw_file;
    size_t    		frame_sz;
} HVC_VRAW_FILE_PARAM_T;


UsageEnvironment* env;
char const* inputFileName = "test.yuv";
H265VideoStreamFramer* videoSource;
RTPSink* videoSink;

API_HVC_BOARD_E eBoard = API_HVC_BOARD_1;
API_HVC_CHN_E eCh = API_HVC_CHN_1;

API_HVC_INIT_PARAM_T tApiHvcInitParam;
HVC_VRAW_FILE_PARAM_T tVrawFileParam;


void *vraw_file_read(void *thread_param_p);
void play(); // forward

void *vraw_file_read(void *thread_param_p)
{
    size_t frame_sz;
    uint8_t *vraw_data_buf_p;
    ssize_t r_size;

    uint64_t total_read_frame;
    uint32_t raw_frame_cnt;
    uint32_t frame_interval;
    HVC_VRAW_FILE_PARAM_T *vraw_param_p;
    uint64_t remain_frame;
    struct stat file_stat;


    frame_sz                = 0;
    vraw_data_buf_p         = NULL;
    total_read_frame        = 0;
    r_size                  = 0;
    total_read_frame        = 0;
    raw_frame_cnt           = 0;
    vraw_param_p            = (HVC_VRAW_FILE_PARAM_T *) thread_param_p;
    remain_frame            = 0;


    fstat(vraw_param_p->fd_vraw_file, &file_stat);

    frame_sz = vraw_param_p->frame_sz;

    vraw_data_buf_p = (uint8_t *) malloc(frame_sz);
    
    if (!vraw_data_buf_p)
    {
        fprintf(stderr, "Error: %s malloc failed!\n", __FILE__);
        return NULL;
    }

    remain_frame = ((uint64_t) file_stat.st_size / frame_sz);

    frame_interval = 1;

    while (remain_frame > 0)
    {
        API_HVC_IMG_T img;

        memset(&img, 0 , sizeof(img));

        r_size = read(vraw_param_p->fd_vraw_file, vraw_data_buf_p, frame_sz);

        if (r_size == 0)
        {
            fprintf(stderr, "Error: read (V-RAW)\n");
            return NULL;
        }
        
        remain_frame--;
        total_read_frame++;
        
        img.pu8Addr     = vraw_data_buf_p;
        img.u32Size     = (uint32_t) frame_sz;
        img.u32Pts      = raw_frame_cnt * frame_interval;
        img.bLastFrame  = (remain_frame == 0) ? true : false;
        img.eFormat     = API_HVC_IMAGE_FORMAT_NV12;

        if (HVC_ENC_PushImage(vraw_param_p->eBoard, vraw_param_p->eCh, &img))
        {
            fprintf(stderr, "Error: %s PushImage failed!\n", __FILE__);
            return NULL;
        }
        
        raw_frame_cnt++;
    }

    if (vraw_data_buf_p != NULL)
    {
        free(vraw_data_buf_p);
    }

    fprintf(stderr, "%s finished!\n", __FUNCTION__);

    return NULL;
}

int main(int argc, char *argv[])
{
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

    if (argc < 3)
    {
        fprintf(stderr, "useage: %s [width] [height]\n", argv[0]);
        
        return -1;
    }

    int wxh;

    wxh = atoi(argv[1]) * atoi(argv[2]) * 3 / 2;

    HVC_ENC_PrintVersion(eBoard);

    tApiHvcInitParam.eInputMode 	= API_HVC_INPUT_MODE_DATA;
    tApiHvcInitParam.eProfile   	= API_HVC_HEVC_MAIN_PROFILE;
    tApiHvcInitParam.eLevel			= API_HVC_HEVC_LEVEL_40;
    tApiHvcInitParam.eTier			= API_HVC_HEVC_MAIN_TIER;
    tApiHvcInitParam.eResolution	= API_HVC_RESOLUTION_720x576;
	tApiHvcInitParam.eChromaFmt     = API_HVC_CHROMA_FORMAT_420;
	tApiHvcInitParam.eBitDepth      = API_HVC_BIT_DEPTH_8;
	tApiHvcInitParam.eGopType       = API_HVC_GOP_IB;
	tApiHvcInitParam.eGopSize       = API_HVC_GOP_SIZE_64;
	tApiHvcInitParam.eBFrameNum     = API_HVC_B_FRAME_MAX;
	tApiHvcInitParam.eTargetFrameRate = API_HVC_FPS_24;
	tApiHvcInitParam.u32Bitrate     = 8000;
	tApiHvcInitParam.eDbgLevel      = API_HVC_DBG_LEVEL_3;

    if (HVC_ENC_Init(eBoard, eCh, &tApiHvcInitParam) == API_HVC_RET_FAIL)
    {
        fprintf(stderr, "%s line %d failed!\n", __FILE__, __LINE__);
        goto main_ret;
    }

	if (HVC_ENC_Start(eBoard, eCh))
	{
		fprintf(stderr, "%s line %d failed!\n", __FILE__, __LINE__);
		goto main_ret;
	}

	// Open image file for pushing.
    int fd;
    fd = open(inputFileName, O_RDONLY);
    if (fd < 0)
    {
        perror(inputFileName);

        goto main_ret;
    }

	pthread_t tid;

	tVrawFileParam.eBoard 	= eBoard;
	tVrawFileParam.eCh		= eCh;
	tVrawFileParam.fd_vraw_file = fd;
	tVrawFileParam.frame_sz = wxh;

	pthread_create
	(
		&tid,
		NULL,
		vraw_file_read,
		&tVrawFileParam
	);

    // Start the streaming:
    *env << "Beginning streaming...\n";
    play();
    
    env->taskScheduler().doEventLoop(); // does not return

main_ret:
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
