/*

Hork Engine Source Code

MIT License

Copyright (C) 2017-2023 Alexander Samusev.

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

#include <Containers/Vector.h>
#include <Geometry/VectorMath.h>
#include <RenderCore/FrameGraph.h>

#include "VT.h"

HK_NAMESPACE_BEGIN

class VirtualTexture;

struct VTCacheLayerInfo
{
    /** Pixel format on GPU */
    TEXTURE_FORMAT TextureFormat;
    /** Upload pixel format */
    RenderCore::DATA_FORMAT UploadFormat;
    /** Page size in bytes for this layer */
    size_t PageSizeInBytes;
};

struct VTCacheCreateInfo
{
    uint32_t PageCacheCapacityX;
    uint32_t PageCacheCapacityY;

    VTCacheLayerInfo* pLayers;
    uint8_t NumLayers;

    uint16_t PageResolutionB;
};

constexpr uint32_t MIN_PAGE_CACHE_CAPACITY = 8;

class VirtualTextureCache : public RefCounted
{
public:
    VirtualTextureCache(VTCacheCreateInfo const& CreateInfo);
    virtual ~VirtualTextureCache();

    bool CreateTexture(const char* FileName, TRef<VirtualTexture>* ppTexture);
    //void DestroyTexture( TRef< VirtualTexture > * ppTexture );

    /** Cache horizontal capacity */
    uint32_t GetPageCacheCapacityX() const { return m_PageCacheCapacityX; }

    /** Cache vertical capacity */
    uint32_t GetPageCacheCapacityY() const { return m_PageCacheCapacityY; }

    /** Cache total capacity */
    uint32_t GetPageCacheCapacity() const { return m_PageCacheCapacity; }

    Float4 const& GetPageTranslationOffsetAndScale() const { return m_PageTranslationOffsetAndScale; }

    /** Page layers in texture memory */
    TVector<TRef<RenderCore::ITexture>>& GetLayers() { return m_PhysCacheLayers; }

    /** Called on every frame */
    void Update();

    void ResetCache();

    struct PageTransfer
    {
        size_t Offset;
        RenderCore::SyncObject Fence;
        VirtualTexture* pTexture;
        uint32_t PageIndex;
        byte* Layers[VT_MAX_LAYERS];
    };

    /** Called by async thread to create new page transfer */
    PageTransfer* CreatePageTransfer();

    /** Called by async thread when page was streamed */
    void MakePageTransferVisible(PageTransfer* Transfer);

    /** Draw cache for debugging */
    void Draw(RenderCore::FrameGraph& FrameGraph, RenderCore::FGTextureProxy* RenderTarget, int LayerIndex);

private:
    bool LockTransfers();

    void UnlockTransfers();

    void TransferPageData(PageTransfer* Transfer, int PhysPageIndex);

    void DiscardTransfers(PageTransfer** Transfers, int Count);

    void WaitForFences();

    /** Physical page cache */
    TVector<TRef<RenderCore::ITexture>> m_PhysCacheLayers;
    TVector<VTCacheLayerInfo> m_LayerInfo;

    using VirtualTexturePtr = VirtualTexture*;

    TVector<VirtualTexturePtr> m_VirtualTextures;

    /** Physical page info */
    struct PhysPageInfo
    {
        /** Time of last request */
        int64_t Time;

        /** Absolute page index */
        uint32_t PageIndex;

        /** Virtual texture */
        VirtualTexture* pTexture;
    };

    /** Physical pages sorted by time */
    struct PhysPageInfoSorted
    {
        PhysPageInfo* pInfo;
    };

    /** Physical page infos */
    TPodVector<PhysPageInfo> m_PhysPageInfo;

    /** Physical page infos sorted by time */
    TPodVector<PhysPageInfoSorted> m_PhysPageInfoSorted;

    uint32_t m_PageCacheCapacityX;
    uint32_t m_PageCacheCapacityY;
    uint32_t m_PageCacheCapacity;
    uint16_t m_PageResolutionB;
    size_t m_PageSizeInBytes;
    size_t m_AlignedSize;
    int m_TotalCachedPages;

    Float4 m_PageTranslationOffsetAndScale;

    int64_t m_LRUTime;

    TPodVector<PageTransfer*> m_Transfers;
    Mutex m_TransfersMutex;

    enum
    {
        MAX_UPLOADS_PER_FRAME = 64
    };
    TRef<RenderCore::IBuffer> m_TransferBuffer;
    byte* m_pTransferData;
    size_t m_TransferDataOffset;
    int m_TransferAllocPoint;
    AtomicInt m_TransferFreePoint;
    PageTransfer m_PageTransfer[MAX_UPLOADS_PER_FRAME];
    SyncEvent m_PageTransferEvent;

    // For debugging
    TRef<RenderCore::IPipeline> m_DrawCachePipeline;
};

HK_NAMESPACE_END
