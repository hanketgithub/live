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
 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 **********/
// "liveMedia"
// Copyright (c) 1996-2015 Live Networks, Inc.  All rights reserved.
// A file source that is a plain byte stream (rather than frames)
// Implementation

#include <stdint.h>
#include <iostream>
#include <string>

#include "m31_hvc_api/HVC_types.h"
#include "m31_hvc_api/HVC_encoder.h"
#include "include/HvcEncoder.hh"

#include "ByteStreamFileSource.hh"
#include "InputFile.hh"
#include "GroupsockHelper.hh"

HvcEncoder *pstEncoder;


////////// ByteStreamFileSource //////////

ByteStreamFileSource*
ByteStreamFileSource::createNew(UsageEnvironment& env, char const* fileName,
                                unsigned preferredFrameSize,
                                unsigned playTimePerFrame)
{
    FILE* fid = OpenInputFile(env, fileName);
    if (fid == NULL) return NULL;
    
    ByteStreamFileSource* newSource
    = new ByteStreamFileSource(env, fid, preferredFrameSize, playTimePerFrame);
    newSource->fFileSize = GetFileSize(fileName, fid);
    
    return newSource;
}

ByteStreamFileSource*
ByteStreamFileSource::createNew(UsageEnvironment& env, FILE* fid,
                                unsigned preferredFrameSize,
                                unsigned playTimePerFrame)
{
    if (fid == NULL) return NULL;
    
    ByteStreamFileSource* newSource = new ByteStreamFileSource(env, fid, preferredFrameSize, playTimePerFrame);
    newSource->fFileSize = GetFileSize(NULL, fid);
    
    return newSource;
}

void ByteStreamFileSource::seekToByteAbsolute(u_int64_t byteNumber, u_int64_t numBytesToStream)
{
    SeekFile64(fFid, (int64_t)byteNumber, SEEK_SET);
    
    fNumBytesToStream = numBytesToStream;
    fLimitNumBytesToStream = fNumBytesToStream > 0;
}

void ByteStreamFileSource::seekToByteRelative(int64_t offset, u_int64_t numBytesToStream)
{
    SeekFile64(fFid, offset, SEEK_CUR);
    
    fNumBytesToStream = numBytesToStream;
    fLimitNumBytesToStream = fNumBytesToStream > 0;
}

void ByteStreamFileSource::seekToEnd()
{
    SeekFile64(fFid, 0, SEEK_END);
}

ByteStreamFileSource::ByteStreamFileSource(UsageEnvironment& env, FILE* fid,
                                           unsigned preferredFrameSize,
                                           unsigned playTimePerFrame)
: FramedFileSource(env, fid), fFileSize(0), fPreferredFrameSize(preferredFrameSize),
fPlayTimePerFrame(playTimePerFrame), fLastPlayTime(0),
fHaveStartedReading(False), fLimitNumBytesToStream(False), fNumBytesToStream(0)
{
#ifndef READ_FROM_FILES_SYNCHRONOUSLY
    makeSocketNonBlocking(fileno(fFid));
#endif
    
    // Test whether the file is seekable
    fFidIsSeekable = FileIsSeekable(fFid);
}

ByteStreamFileSource::~ByteStreamFileSource()
{
    if (fFid == NULL) return;
    
#ifndef READ_FROM_FILES_SYNCHRONOUSLY
    envir().taskScheduler().turnOffBackgroundReadHandling(fileno(fFid));
#endif
    
    CloseInputFile(fFid);
}

void ByteStreamFileSource::doGetNextFrame()
{
    if (feof(fFid) || ferror(fFid) || (fLimitNumBytesToStream && fNumBytesToStream == 0))
    {
        handleClosure();
        return;
    }
    
#ifdef READ_FROM_FILES_SYNCHRONOUSLY
    doReadFromFile();
#else
    if (!fHaveStartedReading)
    {
        // Await readable data from the file:
        envir().taskScheduler().turnOnBackgroundReadHandling(fileno(fFid),
                                                             (TaskScheduler::BackgroundHandlerProc*)&fileReadableHandler, this);
        fHaveStartedReading = True;
    }
#endif
}

void ByteStreamFileSource::doStopGettingFrames()
{
    envir().taskScheduler().unscheduleDelayedTask(nextTask());
#ifndef READ_FROM_FILES_SYNCHRONOUSLY
    envir().taskScheduler().turnOffBackgroundReadHandling(fileno(fFid));
    fHaveStartedReading = False;
#endif
}

void ByteStreamFileSource::fileReadableHandler(ByteStreamFileSource* source, int /*mask*/)
{
    if (!source->isCurrentlyAwaitingData())
    {
        source->doStopGettingFrames(); // we're not ready for the data yet
        return;
    }
    source->doReadFromFile();
}


void ByteStreamFileSource::doReadFromFile()
{
    // Try to read as many bytes as will fit in the buffer provided (or "fPreferredFrameSize" if less)
    if (fLimitNumBytesToStream && fNumBytesToStream < (u_int64_t)fMaxSize)
    {
        fMaxSize = (unsigned)fNumBytesToStream;
    }
    
    if (fPreferredFrameSize > 0 && fPreferredFrameSize < fMaxSize)
    {
        fMaxSize = fPreferredFrameSize;
    }

#ifdef READ_FROM_FILES_SYNCHRONOUSLY
    fFrameSize = fread(fTo, 1, fMaxSize, fFid);
#else
    
    fprintf(stderr, "\nfMaxSize=%d\n", fMaxSize);

    if (fFidIsSeekable)
    {
        //fFrameSize = fread(fTo, 1, fMaxSize, fFid);
        while (1)
        {
            API_HVC_RET ret;
            API_HVC_HEVC_CODED_PICT_T pic;
            static uint8_t tmp[1000000];
            static uint32_t leftBytes;
            
            ret = API_HVC_RET_SUCCESS;
            memset(&pic, 0, sizeof(pic));

            if (leftBytes != 0)
            {
                fprintf(stderr, "Flush %d bytes in tmp!\n", leftBytes);
                memcpy(fTo, tmp, leftBytes);
                fFrameSize = leftBytes;
                leftBytes = 0;
                break;
            }
            else if (pstEncoder->hasLast())
            {
                fprintf(stderr, "Last ES already!\n");
                fFrameSize = 0;

                pstEncoder->stop();
                pstEncoder->exit();

                exit(0);
                
                break;
            }
            else
            {
                ret = pstEncoder->pop(&pic);
                
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

                    std::cout << *pstEncoder->toString(&pic) << std::endl;

                    u32EsSize = p - tmp;

                    //fprintf(stderr, "\n EsSize=%d\n", u32EsSize);

                    if (u32EsSize > fMaxSize)
                    {
                        // I. Copy fMaxSize to fTo
                        memcpy(fTo, tmp, fMaxSize);

                        // II. Copy left bytes to tmp
                        leftBytes = u32EsSize - fMaxSize;
                        memmove(tmp, &tmp[fMaxSize], leftBytes);
                        fFrameSize = fMaxSize;
                    }
                    else
                    {
                        memcpy(fTo, tmp, u32EsSize);
                        fFrameSize = u32EsSize;
                    }
                                        
                    break;
                }
            }    
        }
        
        fprintf(stderr, "send length=%d\n", fFrameSize);
    }
    else
    {
        // For non-seekable files (e.g., pipes), call "read()" rather than "fread()", to ensure that the read doesn't block:
        fFrameSize = read(fileno(fFid), fTo, fMaxSize);
    }
#endif
    if (fFrameSize == 0)
    {
        handleClosure();
        return;
    }
    fNumBytesToStream -= fFrameSize;
    
    // Set the 'presentation time':
    if (fPlayTimePerFrame > 0 && fPreferredFrameSize > 0)
    {
        if (fPresentationTime.tv_sec == 0 && fPresentationTime.tv_usec == 0)
        {
            // This is the first frame, so use the current time:
            gettimeofday(&fPresentationTime, NULL);
        }
        else
        {
            // Increment by the play time of the previous data:
            unsigned uSeconds   = fPresentationTime.tv_usec + fLastPlayTime;
            fPresentationTime.tv_sec += uSeconds/1000000;
            fPresentationTime.tv_usec = uSeconds%1000000;
        }
        
        // Remember the play time of this data:
        fLastPlayTime = (fPlayTimePerFrame*fFrameSize)/fPreferredFrameSize;
        fDurationInMicroseconds = fLastPlayTime;
    }
    else
    {
        // We don't know a specific play time duration for this data,
        // so just record the current time as being the 'presentation time':
        gettimeofday(&fPresentationTime, NULL);
    }
    
    // Inform the reader that he has data:
#ifdef READ_FROM_FILES_SYNCHRONOUSLY
    // To avoid possible infinite recursion, we need to return to the event loop to do this:
    nextTask() = envir().taskScheduler().scheduleDelayedTask(0,
                                                             (TaskFunc*)FramedSource::afterGetting, this);
#else
    // Because the file read was done from the event loop, we can call the
    // 'after getting' function directly, without risk of infinite recursion:
    FramedSource::afterGetting(this);
#endif
}

