#ifndef ___HVC_ENCODER_H___
#define ___HVC_ENCODER_H___

class Encoder
{
public:
    Encoder
    (
        std::ifstream& is,
        int imgSize,
        API_VEGA330X_BOARD_E eBoard,
        API_VEGA330X_CHN_E eCh
    );

    ~Encoder();

    bool init();
    bool start();
    API_HVC_RET push(API_VEGA330X_IMG_T *pImg);
    API_HVC_RET pop(API_VEGA330X_HEVC_CODED_PICT_T *pPic);
    uint32_t fillWithES(uint8_t *dst, uint32_t maxSize);
    bool stop();
    bool exit();

    std::string *toString(API_HVC_HEVC_CODED_PICT_T *pPic);
    
    /* Setter / Getter */
    void setInputMode(API_HVC_INPUT_MODE_E eInputMode);
    API_HVC_INPUT_MODE_E getInputMode();

    void setProfile(API_HVC_HEVC_PROFILE_E eProfile);
    API_HVC_HEVC_PROFILE_E getProfile();

    void setLevel(API_HVC_HEVC_LEVEL_E eLevel);
    API_HVC_HEVC_LEVEL_E getLevel();

    void setTier(API_HVC_HEVC_TIER_E eTier);
    API_HVC_HEVC_TIER_E getTier();

    void setResolution(API_HVC_RESOLUTION_E eRes);
    API_HVC_RESOLUTION_E getResolution();

    void setChromaFormat(API_HVC_CHROMA_FORMAT_E eFmt);
    API_HVC_CHROMA_FORMAT_E getChromaFormat();

    void setBitDepth(API_HVC_BIT_DEPTH_E eBitDepth);
    API_HVC_BIT_DEPTH_E getBitDepth();

    void setGopType(API_HVC_GOP_TYPE_E eType);
    API_HVC_GOP_TYPE_E getGopType();

    void setGopSize(API_HVC_GOP_SIZE_E eSize);
    API_HVC_GOP_SIZE_E getGopSize();

    void setBnum(API_HVC_B_FRAME_NUM_E eSize);
    API_HVC_B_FRAME_NUM_E getBnum();

    void setFps(API_HVC_FPS_E eFps);
    API_HVC_FPS_E getFps();

    void setBitrate(uint32_t u32Bitrate);
    uint32_t getBitrate();

    void setLastES();
    bool hasLastES();
    
private:
    std::ifstream& _inputStream;

    bool        _bLastFramePushed;
    bool        _bLastES;
    int         _imgSize;
    int         _readCnt;
    uint32_t    _leftBytes;
    uint8_t    *_esBuf;

    API_VEGA330X_BOARD_E        _eBoard;
    API_VEGA330X_CHN_E          _eCh;
    API_VEGA330X_INIT_PARAM_T   _apiInitParam;
};

#endif

