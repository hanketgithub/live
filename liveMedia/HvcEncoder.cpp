#include <stdint.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <iostream>

#include "m31_hvc_api/HVC_types.h"
#include "m31_hvc_api/HVC_encoder.h"
#include "include/HvcEncoder.hh"


HvcEncoder::HvcEncoder(API_HVC_BOARD_E eBoard, API_HVC_CHN_E eCh)
{
    this->eBoard = eBoard;
    this->eCh = eCh;
    
    HVC_ENC_PrintVersion(eBoard);
}

bool HvcEncoder::init()
{
    if (HVC_ENC_Init(eBoard, eCh, &tApiHvcInitParam) == API_HVC_RET_FAIL)
    {
        fprintf(stderr, "%s line %d failed!\n", __FILE__, __LINE__);
        return false;
    }

    return true;
}


bool HvcEncoder::start()
{
	if (HVC_ENC_Start(eBoard, eCh))
	{
		fprintf(stderr, "%s line %d failed!\n", __FILE__, __LINE__);
        return false;
	}

    return true;
}


bool HvcEncoder::push(API_HVC_IMG_T *pImg)
{
    if (HVC_ENC_PushImage(eBoard, eCh, pImg))
	{
        return false;
	}

    return true;
}


API_HVC_RET HvcEncoder::pop(API_HVC_HEVC_CODED_PICT_T *pPic)
{
    API_HVC_RET eRet = API_HVC_RET_SUCCESS;
    
    eRet = HVC_ENC_PopES(eBoard, eCh, pPic);

    if (pPic->bLastES)
    {
        bLast = true;
    }

    return eRet;    
}


uint32_t HvcEncoder::read(uint8_t *dst, uint32_t maxSize)
{
    static uint8_t tmp[1000000];
    static uint32_t leftBytes;
    uint32_t u32FrameSize = 0;

    while (1)
    {
        API_HVC_RET ret;
        API_HVC_HEVC_CODED_PICT_T pic;
        
        ret = API_HVC_RET_SUCCESS;
        memset(&pic, 0, sizeof(pic));
    
        if (leftBytes != 0)
        {
            fprintf(stderr, "Flush %d bytes in tmp!\n", leftBytes);
            memcpy(dst, tmp, leftBytes);
            u32FrameSize = leftBytes;
            leftBytes = 0;
            break;
        }
        else
        {
            ret = this->pop(&pic);
            
            if (ret == API_HVC_RET_EMPTY)
            {
                fputc('.', stderr);
            }
            else
            {
                uint32_t j;
                uint8_t *p;
                uint32_t u32EsSize;
    
                p           = tmp;
                u32EsSize   = 0;
                      
                for (j = 0; j < pic.u32NalNum; j++)
                {
                    memcpy(p, pic.tNalInfo[j].pu8Addr, pic.tNalInfo[j].u32Length);
                    p += pic.tNalInfo[j].u32Length;                      
                }
    
                std::cout << *this->toString(&pic) << std::endl;
    
                u32EsSize = p - tmp;
    
                //fprintf(stderr, "\n EsSize=%d\n", u32EsSize);
    
                if (u32EsSize > maxSize)
                {
                    // I. Copy fMaxSize to fTo
                    memcpy(dst, tmp, maxSize);
    
                    // II. Copy left bytes to tmp
                    leftBytes = u32EsSize - maxSize;
                    memmove(tmp, &tmp[maxSize], leftBytes);
                    u32FrameSize = maxSize;
                }
                else
                {
                    memcpy(dst, tmp, u32EsSize);
                    u32FrameSize = u32EsSize;
                }
                                    
                break;
            }
        }    
    }
    
    return u32FrameSize;
}


bool HvcEncoder::stop()
{
    if (HVC_ENC_Stop(eBoard, eCh))
	{
        return false;
	}

    return true;
}


bool HvcEncoder::exit()
{
    if (HVC_ENC_Exit(eBoard, eCh))
	{
        return false;
	}

    return true;
}


std::string *HvcEncoder::toString(API_HVC_HEVC_CODED_PICT_T *pPic)
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
    
    cp += sprintf(cp, " pts=%d", pPic->u32pts);
    cp += sprintf(cp, " last=%d", pPic->bLastES);

    for (int i = 0; i < pPic->u32NalNum; i++)
    {
        cp += sprintf(cp, " NalType=%d", pPic->tNalInfo[i].eNalType);
    }

    return new std::string(msg);    
}


void HvcEncoder::setInputMode(API_HVC_INPUT_MODE_E eInputMode)
{
    this->tApiHvcInitParam.eInputMode = eInputMode;
}


API_HVC_INPUT_MODE_E HvcEncoder::getInputMode()
{
    return this->tApiHvcInitParam.eInputMode;
}


void HvcEncoder::setProfile(API_HVC_HEVC_PROFILE_E eProfile)
{
    this->tApiHvcInitParam.eProfile = eProfile;
}


API_HVC_HEVC_PROFILE_E HvcEncoder::getProfile()
{
    return this->tApiHvcInitParam.eProfile;
}


void HvcEncoder::setLevel(API_HVC_HEVC_LEVEL_E eLevel)
{
    this->tApiHvcInitParam.eLevel = eLevel;
}


API_HVC_HEVC_LEVEL_E HvcEncoder::getLevel()
{
    return this->tApiHvcInitParam.eLevel;
}


void HvcEncoder::setTier(API_HVC_HEVC_TIER_E eTier)
{
    this->tApiHvcInitParam.eTier = eTier;
}


API_HVC_HEVC_TIER_E HvcEncoder::getTier()
{
    return this->tApiHvcInitParam.eTier;
}


void HvcEncoder::setResolution(API_HVC_RESOLUTION_E eRes)
{
    this->tApiHvcInitParam.eResolution = eRes;
}


API_HVC_RESOLUTION_E HvcEncoder::getResolution()
{
    return this->tApiHvcInitParam.eResolution;
}


void HvcEncoder::setChromaFormat(API_HVC_CHROMA_FORMAT_E eFmt)
{
    this->tApiHvcInitParam.eChromaFmt = eFmt;
}


API_HVC_CHROMA_FORMAT_E HvcEncoder::getChromaFormat()
{
    return this->tApiHvcInitParam.eChromaFmt;
}


void HvcEncoder::setBitDepth(API_HVC_BIT_DEPTH_E eBitDepth)
{
    this->tApiHvcInitParam.eBitDepth = eBitDepth;
}


API_HVC_BIT_DEPTH_E HvcEncoder::getBitDepth()
{
    return this->tApiHvcInitParam.eBitDepth;
}


void HvcEncoder::setGopType(API_HVC_GOP_TYPE_E eType)
{
    this->tApiHvcInitParam.eGopType = eType;
}


API_HVC_GOP_TYPE_E HvcEncoder::getGopType()
{
    return this->tApiHvcInitParam.eGopType;
}


void HvcEncoder::setGopSize(API_HVC_GOP_SIZE_E eSize)
{
    this->tApiHvcInitParam.eGopSize = eSize;
}


API_HVC_GOP_SIZE_E HvcEncoder::getGopSize()
{
    return this->tApiHvcInitParam.eGopSize;
}


void HvcEncoder::setBnum(API_HVC_B_FRAME_NUM_E eSize)
{
    this->tApiHvcInitParam.eBFrameNum = eSize;
}


API_HVC_B_FRAME_NUM_E HvcEncoder::getBnum()
{
    return this->tApiHvcInitParam.eBFrameNum;
}


void HvcEncoder::setFps(API_HVC_FPS_E eFps)
{
    this->tApiHvcInitParam.eTargetFrameRate = eFps;
}


API_HVC_FPS_E HvcEncoder::getFps()
{
    return this->tApiHvcInitParam.eTargetFrameRate;
}


void HvcEncoder::setBitrate(uint32_t u32Bitrate)
{
    this->tApiHvcInitParam.u32Bitrate = u32Bitrate;
}


uint32_t HvcEncoder::getBitrate()
{
    return this->tApiHvcInitParam.u32Bitrate;
}


void HvcEncoder::setLast()
{
    this->bLast = true;
}


bool HvcEncoder::hasLast()
{
    return this->bLast;
}


