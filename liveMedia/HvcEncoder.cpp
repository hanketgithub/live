#include <stdint.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <iostream>
#include <fstream>

#include <libvega_encoder_api/VEGA330X_types.h>
#include <libvega_encoder_api/VEGA330X_encoder.h>
#include "include/HvcEncoder.hh"


Encoder::Encoder(API_HVC_BOARD_E eBoard, API_HVC_CHN_E eCh)
{
    this->eBoard = eBoard;
    this->eCh = eCh;
    
    VEGA330X_ENC_PrintVersion(eBoard);
}

bool Encoder::init()
{
    tApiInitParam.eDbgLevel = API_VEGA330X_DBG_LEVEL_3;
    tApiInitParam.bDisableMonitor = true;

    if (VEGA330X_ENC_Init(eBoard, eCh, &tApiInitParam) == API_HVC_RET_FAIL)
    {
        fprintf(stderr, "%s line %d failed!\n", __FILE__, __LINE__);
        return false;
    }

    return true;
}


bool Encoder::start()
{
	if (VEGA330X_ENC_Start(eBoard, eCh))
	{
		fprintf(stderr, "%s line %d failed!\n", __FILE__, __LINE__);
        return false;
	}

    return true;
}


API_HVC_RET Encoder::push(API_VEGA330X_IMG_T *pImg)
{
    return VEGA330X_ENC_PushImage(eBoard, eCh, pImg);
}


API_HVC_RET Encoder::pop(API_VEGA330X_HEVC_CODED_PICT_T *pPic)
{
    API_HVC_RET eRet = API_HVC_RET_SUCCESS;
    
    eRet = VEGA330X_ENC_PopES(eBoard, eCh, pPic);

    if (pPic->bLastES)
    {
        this->bLastES = true;
    }

    return eRet;    
}

/** Major Encoder input/output function         */
/** Do push-pop pair to generate ES             */
/** Must have pushed 9 raw frame before head    */
uint32_t Encoder::fillWithES(uint8_t *dst, uint32_t maxSize)
{
    static uint8_t read_buf[1000000];
    static uint32_t leftBytes;
    static int frameCnt = 0;
    uint32_t u32FrameSize = 0;


    if (leftBytes != 0)
    {
        fprintf(stderr, "Flush %d bytes in buffer!\n", leftBytes);
        memcpy(dst, read_buf, leftBytes);
        u32FrameSize = leftBytes;
        leftBytes = 0;
    }
    else
    {
        /* Do push image */
        uint8_t *vraw_data_buf_p; 
        API_HVC_IMG_T img;

        memset(&img, 0, sizeof(img));
        vraw_data_buf_p = (uint8_t *) calloc(this->img_size, sizeof(uint8_t));

        this->_inputStream->read((char*) vraw_data_buf_p, this->img_size);
        frameCnt++;
        if (!this->bLastFramePushed)
        {
            img.pu8Addr     = vraw_data_buf_p;
            img.u32Size     = (uint32_t) this->img_size;
            img.bLastFrame  = (frameCnt == 200);
            img.eFormat     = API_HVC_IMAGE_FORMAT_NV12;

            this->push(&img);
        }
        if (img.bLastFrame)
        {
            this->bLastFramePushed = true;
        }

        free(vraw_data_buf_p);
        
        /* Do pop ES */
        API_VEGA330X_HEVC_CODED_PICT_T coded_pict;
        
        memset(&coded_pict, 0, sizeof(coded_pict));
        
        this->pop(&coded_pict);

        uint32_t j;
        uint8_t *p;
        uint32_t u32EsSize;

        p           = read_buf;
        u32EsSize   = 0;
              
        for (j = 0; j < coded_pict.u32NalNum; j++)
        {
            memcpy(p, coded_pict.tNalInfo[j].pu8Addr, coded_pict.tNalInfo[j].u32Length);
            p += coded_pict.tNalInfo[j].u32Length;                      
        }

        std::cout << *this->toString(&coded_pict) << std::endl;

        u32EsSize = p - read_buf;

        //fprintf(stderr, "\n EsSize=%d\n", u32EsSize);

        if (u32EsSize > maxSize)
        {
            // I. Copy fMaxSize to fTo
            memcpy(dst, read_buf, maxSize);

            // II. Copy left bytes to read_buf
            leftBytes = u32EsSize - maxSize;
            memmove(read_buf, &read_buf[maxSize], leftBytes);
            u32FrameSize = maxSize;
        }
        else
        {
            memcpy(dst, read_buf, u32EsSize);
            u32FrameSize = u32EsSize;
        }
    }    
    
    return u32FrameSize;
}


bool Encoder::stop()
{
    if (HVC_ENC_Stop(eBoard, eCh))
	{
        return false;
	}

    return true;
}


bool Encoder::exit()
{
    if (HVC_ENC_Exit(eBoard, eCh))
	{
        return false;
	}

    return true;
}


std::string *Encoder::toString(API_HVC_HEVC_CODED_PICT_T *pPic)
{
    char msg[128];
    char *cp = msg;

    switch (pPic->eFrameType)
    {
        case API_HVC_FRAME_TYPE_I:
        {
            cp += sprintf(cp, "'I'");
            
            break;
        }
        case API_HVC_FRAME_TYPE_P:
        {
            cp += sprintf(cp, "'P'");
            
            break;
        }
        case API_HVC_FRAME_TYPE_B:
        {
            cp += sprintf(cp, "'B'");

            break;
        }
        default:
        {
            cp += sprintf(cp, "'?'");

            break;
        }
    }
    
    cp += sprintf(cp, " pts=%ld", pPic->pts);
    cp += sprintf(cp, " last=%d", pPic->bLastES);

    for (uint32_t i = 0; i < pPic->u32NalNum; i++)
    {
        cp += sprintf(cp, " NalType=%d", pPic->tNalInfo[i].eNalType);
    }

    return new std::string(msg);    
}


void Encoder::setInputMode(API_HVC_INPUT_MODE_E eInputMode)
{
    this->tApiInitParam.eInputMode = eInputMode;
}


API_HVC_INPUT_MODE_E Encoder::getInputMode()
{
    return this->tApiInitParam.eInputMode;
}


void Encoder::setProfile(API_HVC_HEVC_PROFILE_E eProfile)
{
    this->tApiInitParam.eProfile = eProfile;
}


API_HVC_HEVC_PROFILE_E Encoder::getProfile()
{
    return this->tApiInitParam.eProfile;
}


void Encoder::setLevel(API_HVC_HEVC_LEVEL_E eLevel)
{
    this->tApiInitParam.eLevel = eLevel;
}


API_HVC_HEVC_LEVEL_E Encoder::getLevel()
{
    return this->tApiInitParam.eLevel;
}


void Encoder::setTier(API_HVC_HEVC_TIER_E eTier)
{
    this->tApiInitParam.eTier = eTier;
}


API_HVC_HEVC_TIER_E Encoder::getTier()
{
    return this->tApiInitParam.eTier;
}


void Encoder::setResolution(API_HVC_RESOLUTION_E eRes)
{
    this->tApiInitParam.eResolution = eRes;
}


API_HVC_RESOLUTION_E Encoder::getResolution()
{
    return this->tApiInitParam.eResolution;
}


void Encoder::setChromaFormat(API_HVC_CHROMA_FORMAT_E eFmt)
{
    this->tApiInitParam.eChromaFmt = eFmt;
}


API_HVC_CHROMA_FORMAT_E Encoder::getChromaFormat()
{
    return this->tApiInitParam.eChromaFmt;
}


void Encoder::setBitDepth(API_HVC_BIT_DEPTH_E eBitDepth)
{
    this->tApiInitParam.eBitDepth = eBitDepth;
}


API_HVC_BIT_DEPTH_E Encoder::getBitDepth()
{
    return this->tApiInitParam.eBitDepth;
}


void Encoder::setGopType(API_HVC_GOP_TYPE_E eType)
{
    this->tApiInitParam.eGopType = eType;
}


API_HVC_GOP_TYPE_E Encoder::getGopType()
{
    return this->tApiInitParam.eGopType;
}


void Encoder::setGopSize(API_HVC_GOP_SIZE_E eSize)
{
    this->tApiInitParam.eGopSize = eSize;
}


API_HVC_GOP_SIZE_E Encoder::getGopSize()
{
    return this->tApiInitParam.eGopSize;
}


void Encoder::setBnum(API_HVC_B_FRAME_NUM_E eSize)
{
    this->tApiInitParam.eBFrameNum = eSize;
}


API_HVC_B_FRAME_NUM_E Encoder::getBnum()
{
    return this->tApiInitParam.eBFrameNum;
}


void Encoder::setFps(API_HVC_FPS_E eFps)
{
    this->tApiInitParam.eTargetFrameRate = eFps;
}


API_HVC_FPS_E Encoder::getFps()
{
    return this->tApiInitParam.eTargetFrameRate;
}


void Encoder::setBitrate(uint32_t u32Bitrate)
{
    this->tApiInitParam.u32Bitrate = u32Bitrate;
}


uint32_t Encoder::getBitrate()
{
    return this->tApiInitParam.u32Bitrate;
}


void Encoder::setLastES()
{
    this->bLastES = true;
}


bool Encoder::hasLastES()
{
    return this->bLastES;
}


void Encoder::setInputStream(std::ifstream *is)
{
    this->_inputStream = is;
}

void Encoder::setImgSize(int sz)
{
    this->img_size = sz;
}

