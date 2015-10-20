#ifndef ___HVC_ENCODER_H___
#define ___HVC_ENCODER_H___

class HvcEncoder
{
public:
    HvcEncoder(API_HVC_BOARD_E eBoard, API_HVC_CHN_E eCh);

    bool init();
    bool start();
    bool push(API_HVC_IMG_T *pImg);
    API_HVC_RET pop(API_HVC_HEVC_CODED_PICT_T *pPic);
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

    void setLast();
    bool hasLast();
    
private:
    bool bLast;
    
    API_HVC_BOARD_E         eBoard;
    API_HVC_CHN_E           eCh;
    API_HVC_INIT_PARAM_T    tApiHvcInitParam;
};

#endif

