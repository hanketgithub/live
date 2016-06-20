#include <stdint.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <iostream>
#include <fstream>

#include <libvega_encoder_api/VEGA330X_types.h>
#include <libvega_encoder_api/VEGA330X_encoder.h>
#include "include/HvcEncoder.hh"

using namespace std;

Encoder::Encoder
(
    ifstream& is,
    int imgSize,
    API_VEGA330X_BOARD_E eBoard,
    API_VEGA330X_CHN_E eCh
) : _inputStream(is), _imgSize(imgSize)
{
    this->_eBoard = eBoard;
    this->_eCh = eCh;
    this->_esBuf = (uint8_t *) calloc(1000000, 1);
    
    VEGA330X_ENC_PrintVersion(eBoard);
}

Encoder::~Encoder()
{
    free(this->_esBuf);
}

bool Encoder::init()
{
    _apiInitParam.eDbgLevel = API_VEGA330X_DBG_LEVEL_0;
    _apiInitParam.bDisableMonitor = true;

    if (VEGA330X_ENC_Init(_eBoard, _eCh, &_apiInitParam) == API_HVC_RET_FAIL)
    {
        cout << __FILE__ << " line " << __LINE__ << " failed!" << endl;
        return false;
    }

    return true;
}


bool Encoder::start()
{
	if (VEGA330X_ENC_Start(_eBoard, _eCh))
	{
        cout << __FILE__ << " line " << __LINE__ << " failed!" << endl;
        return false;
	}

    return true;
}


API_HVC_RET Encoder::push(API_VEGA330X_IMG_T *pImg)
{
    return VEGA330X_ENC_PushImage(_eBoard, _eCh, pImg);
}


API_HVC_RET Encoder::pop(API_VEGA330X_HEVC_CODED_PICT_T *pPic)
{
    API_HVC_RET eRet = API_HVC_RET_SUCCESS;
    
    eRet = VEGA330X_ENC_PopES(_eBoard, _eCh, pPic);

    if (pPic->bLastES)
    {
        this->_bLastES = true;
    }

    return eRet;    
}

/** Major Encoder input/output function         */
/** Do push-pop pair to generate ES             */
/** Must have pushed 9 raw frame beforehand     */
uint32_t Encoder::fillWithES(uint8_t *dst, uint32_t maxSize)
{
    uint32_t u32FrameSize = 0;
    //static uint8_t _esBuf[1000000];
    
    if (_leftBytes != 0)
    {
        fprintf(stderr, "Flush %d bytes in buffer!\n", _leftBytes);
        memcpy(dst, _esBuf, _leftBytes);
        u32FrameSize = _leftBytes;
        _leftBytes = 0;
    }
    else
    {
        /* Do push image */
        uint8_t *vraw_data_buf_p; 
        API_HVC_IMG_T img;

        memset(&img, 0, sizeof(img));
   
        vraw_data_buf_p = (uint8_t *) calloc(this->_imgSize, sizeof(uint8_t));
        _inputStream.read((char*) vraw_data_buf_p, this->_imgSize);

        if (!_inputStream.eof())
        {
            this->_readCnt++;
            cout << this->_readCnt << " read success!" << endl;
            if (!this->_bLastFramePushed)
            {
                img.pu8Addr     = vraw_data_buf_p;
                img.u32Size     = (uint32_t) this->_imgSize;
                img.bLastFrame  = false;
                img.eFormat     = API_VEGA330X_IMAGE_FORMAT_YUV420;

                this->push(&img);
            }
        }
        else if (!this->_bLastFramePushed)
        {
            img.pu8Addr     = vraw_data_buf_p;
            img.u32Size     = (uint32_t) this->_imgSize;
            img.bLastFrame  = true;
            img.eFormat     = API_VEGA330X_IMAGE_FORMAT_YUV420;
            
            this->push(&img);

            this->_bLastFramePushed = true;
        }
        free(vraw_data_buf_p);
        
        /* Do pop ES */
        API_VEGA330X_HEVC_CODED_PICT_T coded_pict;
        
        memset(&coded_pict, 0, sizeof(coded_pict));
        
        this->pop(&coded_pict);

        uint32_t j;
        uint8_t *p;
        uint32_t u32EsSize;

        p           = _esBuf;
        u32EsSize   = 0;
              
        for (j = 0; j < coded_pict.u32NalNum; j++)
        {
            memcpy(p, coded_pict.tNalInfo[j].pu8Addr, coded_pict.tNalInfo[j].u32Length);
            p += coded_pict.tNalInfo[j].u32Length;                      
        }

        std::cout << *this->toString(&coded_pict) << std::endl;

        u32EsSize = p - _esBuf;

        //fprintf(stderr, "\n EsSize=%d\n", u32EsSize);

        if (u32EsSize > maxSize)
        {
            // I. Copy fMaxSize to fTo
            memcpy(dst, _esBuf, maxSize);

            // II. Copy left bytes to _esBuf
            _leftBytes = u32EsSize - maxSize;
            memmove(_esBuf, &_esBuf[maxSize], _leftBytes);
            u32FrameSize = maxSize;
        }
        else
        {
            memcpy(dst, _esBuf, u32EsSize);
            u32FrameSize = u32EsSize;
        }
    }    
    
    return u32FrameSize;
}


bool Encoder::stop()
{
    if (HVC_ENC_Stop(_eBoard, _eCh))
	{
        return false;
	}

    return true;
}


bool Encoder::exit()
{
    if (HVC_ENC_Exit(_eBoard, _eCh))
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
    this->_apiInitParam.eInputMode = eInputMode;
}


API_HVC_INPUT_MODE_E Encoder::getInputMode()
{
    return this->_apiInitParam.eInputMode;
}


void Encoder::setProfile(API_HVC_HEVC_PROFILE_E eProfile)
{
    this->_apiInitParam.eProfile = eProfile;
}


API_HVC_HEVC_PROFILE_E Encoder::getProfile()
{
    return this->_apiInitParam.eProfile;
}


void Encoder::setLevel(API_HVC_HEVC_LEVEL_E eLevel)
{
    this->_apiInitParam.eLevel = eLevel;
}


API_HVC_HEVC_LEVEL_E Encoder::getLevel()
{
    return this->_apiInitParam.eLevel;
}


void Encoder::setTier(API_HVC_HEVC_TIER_E eTier)
{
    this->_apiInitParam.eTier = eTier;
}


API_HVC_HEVC_TIER_E Encoder::getTier()
{
    return this->_apiInitParam.eTier;
}


void Encoder::setResolution(API_HVC_RESOLUTION_E eRes)
{
    this->_apiInitParam.eResolution = eRes;
}


API_HVC_RESOLUTION_E Encoder::getResolution()
{
    return this->_apiInitParam.eResolution;
}


void Encoder::setChromaFormat(API_HVC_CHROMA_FORMAT_E eFmt)
{
    this->_apiInitParam.eChromaFmt = eFmt;
}


API_HVC_CHROMA_FORMAT_E Encoder::getChromaFormat()
{
    return this->_apiInitParam.eChromaFmt;
}


void Encoder::setBitDepth(API_HVC_BIT_DEPTH_E eBitDepth)
{
    this->_apiInitParam.eBitDepth = eBitDepth;
}


API_HVC_BIT_DEPTH_E Encoder::getBitDepth()
{
    return this->_apiInitParam.eBitDepth;
}


void Encoder::setGopType(API_HVC_GOP_TYPE_E eType)
{
    this->_apiInitParam.eGopType = eType;
}


API_HVC_GOP_TYPE_E Encoder::getGopType()
{
    return this->_apiInitParam.eGopType;
}


void Encoder::setGopSize(API_HVC_GOP_SIZE_E eSize)
{
    this->_apiInitParam.eGopSize = eSize;
}


API_HVC_GOP_SIZE_E Encoder::getGopSize()
{
    return this->_apiInitParam.eGopSize;
}


void Encoder::setBnum(API_HVC_B_FRAME_NUM_E eSize)
{
    this->_apiInitParam.eBFrameNum = eSize;
}


API_HVC_B_FRAME_NUM_E Encoder::getBnum()
{
    return this->_apiInitParam.eBFrameNum;
}


void Encoder::setFps(API_HVC_FPS_E eFps)
{
    this->_apiInitParam.eTargetFrameRate = eFps;
}


API_HVC_FPS_E Encoder::getFps()
{
    return this->_apiInitParam.eTargetFrameRate;
}


void Encoder::setBitrate(uint32_t u32Bitrate)
{
    this->_apiInitParam.u32Bitrate = u32Bitrate;
}


uint32_t Encoder::getBitrate()
{
    return this->_apiInitParam.u32Bitrate;
}


void Encoder::setLastES()
{
    this->_bLastES = true;
}


bool Encoder::hasLastES()
{
    return this->_bLastES;
}

