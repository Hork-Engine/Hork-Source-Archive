/*

Hork Engine Source Code

MIT License

Copyright (C) 2017-2022 Alexander Samusev.

This file is part of the Hork Engine Source Code.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#pragma once

#include "AudioDevice.h"
#include "AudioChannel.h"

#include <Containers/Vector.h>
#include <Core/ConsoleVar.h>

class AudioMixer
{
    HK_FORBID_COPY(AudioMixer)

public:
    AudioMixer(AudioDevice* _Device);
    virtual ~AudioMixer();

    /** Make channel visible for mixer thread */
    void SubmitChannel(AudioChannel* Channel);

    /** Get current active channels */
    int GetNumActiveChannels() const
    {
        return NumActiveChannels.Load();
    }

    /** Get number of not active (virtual) channels */
    int GetNumVirtualChannels() const
    {
        return TotalChannels.Load() - NumActiveChannels.Load();
    }

    /** Get total count of channels */
    int GetTotalChannels() const
    {
        return TotalChannels.Load();
    }

    /** Start async mixing */
    void StartAsync();

    /** Stop async mixing */
    void StopAsync();

    /** Perform mixing in main thread */
    void Update();

    bool IsAsync() const
    {
        return bAsync;
    }

private:
    struct SamplePair
    {
        union
        {
            int32_t Chan[2];
            float Chanf[2];
        };
    };

    void UpdateAsync(uint8_t* pTransferBuffer, int TransferBufferSizeInFrames, int FrameNum, int MinFramesToRender);

    // This fuction adds pending channels to list
    void AddPendingChannels();
    void RejectChannel(AudioChannel* Channel);
    void RenderChannels(int64_t EndFrame);
    void RenderChannel(AudioChannel* Chan, int64_t EndFrame);
    void RenderStream(AudioChannel* Chan, int64_t EndFrame);
    void RenderFramesHRTF(AudioChannel* Chan, int FrameCount, SamplePair* pBuffer);
    void RenderFrames(AudioChannel* Chan, const void* pFrames, int FrameCount, SamplePair* pBuffer);
    void WriteToTransferBuffer(int const* pSamples, int64_t EndFrame);
    void MakeVolumeRamp(const int CurVol[2], const int NewVol[2], int FrameCount, int Scale);
    void ReadFramesF32(AudioChannel* Chan, int FramesToRead, int HistoryExtraFrames, float* pFrames);

    TUniqueRef<class AudioHRTF> Hrtf;
    TUniqueRef<class Freeverb> ReverbFilter;

    alignas(16) SamplePair RenderBuffer[2048];
    const int RenderBufferSize = HK_ARRAY_SIZE(RenderBuffer);

    TRef<AudioDevice> pDevice;
    AudioDevice* DeviceRawPtr;
    uint8_t* pTransferBuffer;
    bool bAsync;
    int64_t RenderFrame;
    AtomicInt NumActiveChannels;
    AtomicInt TotalChannels;

    AudioChannel* Channels;
    AudioChannel* ChannelsTail;
    AudioChannel* PendingList;
    AudioChannel* PendingListTail;

    SpinLock SubmitLock;

    // For current mixing channel
    int NewVol[2];
    Float3 NewDir;
    bool bSpatializedChannel;
    bool bChannelPaused;
    int PlaybackPos;
    int VolumeRampL[1024];
    int VolumeRampR[1024];
    int VolumeRampSize;

    TVector<uint8_t> TempFrames;
    TVector<float> FramesF32;
    TVector<SamplePair> StreamF32;
};

extern ConsoleVar Snd_HRTF;
