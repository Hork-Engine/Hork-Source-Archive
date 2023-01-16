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

#include <Platform/Memory/Memory.h>
#include <Platform/Logger.h>

HK_NAMESPACE_BEGIN

/**

TPodQueue

Queue for POD types

*/
template <typename T, int BaseCapacity = 256, bool bEnableOverflow = false, typename Allocator = Allocators::HeapMemoryAllocator<HEAP_VECTOR>>
class TPodQueue final
{
public:
    static constexpr size_t TYPE_SIZE = sizeof(T);

    TPodQueue() :
        m_pQueue(StaticData), m_QueueHead(0), m_QueueTail(0), m_MaxQueueLength(BaseCapacity)
    {
        static_assert(IsPowerOfTwo(BaseCapacity), "Queue length must be power of two");
    }

    TPodQueue(TPodQueue const& _Queue)
    {
        if (_Queue.m_MaxQueueLength > BaseCapacity)
        {
            m_MaxQueueLength = _Queue.m_MaxQueueLength;
            m_pQueue         = (T*)Allocator().allocate(TYPE_SIZE * m_MaxQueueLength);
        }
        else
        {
            m_MaxQueueLength = BaseCapacity;
            m_pQueue       = StaticData;
        }

        const int queueLength = _Queue.Size();
        if (queueLength == _Queue.m_MaxQueueLength || _Queue.m_QueueTail == 0)
        {
            Platform::Memcpy(m_pQueue, _Queue.m_pQueue, TYPE_SIZE * queueLength);
            m_QueueHead = _Queue.m_QueueHead;
            m_QueueTail = _Queue.m_QueueTail;
        }
        else
        {
            const int WrapMask = _Queue.m_MaxQueueLength - 1;
            for (int i = 0; i < queueLength; i++)
            {
                m_pQueue[i] = _Queue.m_pQueue[(i + _Queue.m_QueueTail) & WrapMask];
            }
            m_QueueHead = queueLength;
            m_QueueTail = 0;
        }
    }

    ~TPodQueue()
    {
        if (m_pQueue != StaticData)
        {
            Allocator().deallocate(m_pQueue);
        }
    }

    T* Head() const
    {
        if (IsEmpty())
        {
            return nullptr;
        }
        return m_pQueue[(m_QueueHead - 1) & (m_MaxQueueLength - 1)];
    }

    T* Tail() const
    {
        if (IsEmpty())
        {
            return nullptr;
        }
        return m_pQueue[m_QueueTail & (m_MaxQueueLength - 1)];
    }

    T* Push()
    {
        if (m_QueueHead - m_QueueTail < m_MaxQueueLength)
        {
            m_QueueHead++;
            return &m_pQueue[(m_QueueHead - 1) & (m_MaxQueueLength - 1)];
        }

        if (!bEnableOverflow)
        {
            LOG("TPodQueue::Push: queue overflow\n");
            m_QueueTail++;
            m_QueueHead++;
            return &m_pQueue[(m_QueueHead - 1) & (m_MaxQueueLength - 1)];
        }

        const int WrapMask = m_MaxQueueLength - 1;

        m_MaxQueueLength <<= 1;

        const int queueLength = Size();
        if (m_QueueTail == 0)
        {
            if (m_pQueue == StaticData)
            {
                m_pQueue = (T*)Allocator().allocate(TYPE_SIZE * m_MaxQueueLength);
                Platform::Memcpy(m_pQueue, StaticData, TYPE_SIZE * queueLength);
            }
            else
            {
                m_pQueue = (T*)Allocator().reallocate(m_pQueue, TYPE_SIZE * m_MaxQueueLength, true);
            }
        }
        else
        {
            T* data = (T*)Allocator().allocate(TYPE_SIZE * m_MaxQueueLength);
            for (int i = 0; i < queueLength; i++)
            {
                data[i] = m_pQueue[(i + m_QueueTail) & WrapMask];
            }
            m_QueueHead = queueLength;
            m_QueueTail = 0;
            if (m_pQueue != StaticData)
            {
                Allocator().deallocate(m_pQueue);
            }
            m_pQueue = data;
        }

        m_QueueHead++;
        return &m_pQueue[(m_QueueHead - 1) & (m_MaxQueueLength - 1)];
    }

    T* Pop()
    {
        if (m_QueueHead > m_QueueTail)
        {
            m_QueueTail++;
            return &m_pQueue[(m_QueueTail - 1) & (m_MaxQueueLength - 1)];
        }
        return nullptr;
    }

    T* PopFront()
    {
        if (m_QueueHead > m_QueueTail)
        {
            m_QueueHead--;
            return &m_pQueue[m_QueueHead & (m_MaxQueueLength - 1)];
        }
        return nullptr;
    }

    bool IsEmpty() const
    {
        return m_QueueHead == m_QueueTail;
    }

    void Clear()
    {
        m_QueueHead = m_QueueTail = 0;
    }

    void Free()
    {
        Clear();
        if (m_pQueue != StaticData)
        {
            Allocator::Inst().Free(m_pQueue);
            m_pQueue = StaticData;
        }
        m_MaxQueueLength = BaseCapacity;
    }

    int Size() const
    {
        return m_QueueHead - m_QueueTail;
    }

    int Capacity() const
    {
        return m_MaxQueueLength;
    }

    TPodQueue& operator=(TPodQueue const& _Queue)
    {
        // Resize queue
        if (_Queue.Size() > m_MaxQueueLength)
        {
            if (m_pQueue != StaticData)
            {
                Allocator::Inst().Free(m_pQueue);
            }
            if (_Queue.m_MaxQueueLength > BaseCapacity)
            {
                m_MaxQueueLength = _Queue.m_MaxQueueLength;
                m_pQueue         = (T*)Allocator().allocate(TYPE_SIZE * m_MaxQueueLength);
            }
            else
            {
                m_MaxQueueLength = BaseCapacity;
                m_pQueue       = StaticData;
            }
        }

        // Copy
        const int queueLength = _Queue.Size();
        if (queueLength == _Queue.m_MaxQueueLength || _Queue.m_QueueTail == 0)
        {
            Platform::Memcpy(m_pQueue, _Queue.m_pQueue, TYPE_SIZE * queueLength);
            m_QueueHead = _Queue.m_QueueHead;
            m_QueueTail = _Queue.m_QueueTail;
        }
        else
        {
            const int WrapMask = _Queue.m_MaxQueueLength - 1;
            for (int i = 0; i < queueLength; i++)
            {
                m_pQueue[i] = _Queue.m_pQueue[(i + _Queue.m_QueueTail) & WrapMask];
            }
            m_QueueHead = queueLength;
            m_QueueTail = 0;
        }

        return *this;
    }

private:
    static_assert(std::is_trivial<T>::value, "Expected POD type");

    alignas(16) T StaticData[BaseCapacity];
    T*  m_pQueue;
    int m_QueueHead;
    int m_QueueTail;
    int m_MaxQueueLength;
};

HK_NAMESPACE_END
