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

#include "AsyncJobManager.h"
#include <Platform/Logger.h>

constexpr int AAsyncJobManager::MAX_WORKER_THREADS;
constexpr int AAsyncJobManager::MAX_JOB_LISTS;

AAsyncJobManager::AAsyncJobManager(int _NumWorkerThreads, int _NumJobLists)
{
    if (_NumWorkerThreads > MAX_WORKER_THREADS)
    {
        LOG("AAsyncJobManager::Initialize: NumWorkerThreads > MAX_WORKER_THREADS\n");
        _NumWorkerThreads = MAX_WORKER_THREADS;
    }
    else if (_NumWorkerThreads <= 0)
    {
        _NumWorkerThreads = MAX_WORKER_THREADS;
    }

    HK_ASSERT(_NumJobLists >= 1 && _NumJobLists <= MAX_JOB_LISTS);

    LOG("Initializing async job manager ( {} worker threads, {} job lists )\n", _NumWorkerThreads, _NumJobLists);

    bTerminated = false;

    NumJobLists = _NumJobLists;
    for (int i = 0; i < NumJobLists; i++)
    {
        JobList[i].JobManager = this;
    }

    TotalJobs.Store(0);

    NumWorkerThreads = _NumWorkerThreads;
    for (int i = 0; i < NumWorkerThreads; i++)
    {
        WorkerThread[i] = AThread(
            [this](int ThreadId)
            {
                WorkerThreadRoutine(ThreadId);
            },
            i);
    }
}

AAsyncJobManager::~AAsyncJobManager()
{
    LOG("Deinitializing async job manager\n");

    NotifyThreads();

    for (int i = 0; i < NumJobLists; i++)
    {
        JobList[i].Wait();
        JobList[i].JobPool.Free();
    }

    bTerminated = true;
    NotifyThreads();

    for (int i = 0; i < NumWorkerThreads; i++)
    {
        WorkerThread[i].Join();
    }
}

void AAsyncJobManager::NotifyThreads()
{
    for (int i = 0; i < NumWorkerThreads; i++)
    {
        EventNotify[i].Signal();
    }
}

void AAsyncJobManager::WorkerThreadRoutine(int _ThreadId)
{
    SAsyncJob job = {};
    bool      haveJob;

#ifdef HK_ACTIVE_THREADS_COUNTERS
    NumActiveThreads.Increment();
#endif

    while (!bTerminated)
    {

#ifdef HK_ACTIVE_THREADS_COUNTERS
        NumActiveThreads.Decrement();
#endif

        //LOG( "Thread waiting {}\n", _ThreadId );

        EventNotify[_ThreadId].Wait();

#ifdef HK_ACTIVE_THREADS_COUNTERS
        NumActiveThreads.Increment();
#endif

        for (int currentList = 0; TotalJobs.Load() > 0; currentList++)
        {

            int fetchIndex = (_ThreadId + currentList) % NumJobLists;

            AAsyncJobList* jobList = &JobList[fetchIndex];

            // Check if list have a jobs
            if (jobList->FetchCount.Load() > 0)
            {

                haveJob = false;

                // fetch job
                {
                    AMutexGurad syncGuard(jobList->SubmitSync);

                    //if ( jobList->FetchLock.Increment() == 1 )
                    //{
                    if (jobList->SubmittedJobs)
                    {
                        job                    = *jobList->SubmittedJobs;
                        jobList->SubmittedJobs = job.Next;
                        haveJob                = true;

                        jobList->FetchCount.Decrement();
                        TotalJobs.Decrement();
                    }
                    //    jobList->FetchLock.Decrement();
                    //} else {
                    //    jobList->FetchLock.Decrement();
                    //}
                }

                if (haveJob)
                {
                    job.Callback(job.Data);

                    // Check if this was last processed job in the list
                    if (jobList->SubmittedJobsCount.Decrement() == 0)
                    {
                        AMutexGurad syncGuard(jobList->SubmitSync);

                        // Check for new submits
                        if (!jobList->SubmittedJobs && jobList->SubmittedJobsCount.Load() == 0)
                        {

                            // Check if already signalled from other thread
                            if (!jobList->bSignalled)
                            {
                                jobList->bSignalled = true;
                                jobList->EventDone.Signal();
                            }
                        }
                    }
                }
            }
        }
    }

#ifdef HK_ACTIVE_THREADS_COUNTERS
    NumActiveThreads.Decrement();
#endif

    LOG("Terminating worker thread ({})\n", _ThreadId);
}

AAsyncJobList::AAsyncJobList()
{
}

AAsyncJobList::~AAsyncJobList()
{
    Wait();
}

void AAsyncJobList::SetMaxParallelJobs(int _MaxParallelJobs)
{
    HK_ASSERT(JobPool.IsEmpty());

    JobPool.Clear();
    JobPool.Reserve(_MaxParallelJobs);    
}

void AAsyncJobList::AddJob(void (*_Callback)(void*), void* _Data)
{
    if (JobPool.Size() == JobPool.Capacity())
    {
        LOG("Warning: AAsyncJobList::AddJob: job pool overflow, use SetMaxParallelJobs to reserve proper pool size (current size {})\n", JobPool.Capacity());

        SubmitAndWait();
        SetMaxParallelJobs(JobPool.Capacity() * 2);
    }

    SAsyncJob& job = JobPool.Add();
    job.Callback   = _Callback;
    job.Data       = _Data;
    job.Next       = JobList;
    JobList        = &job;
    NumPendingJobs++;
}

void AAsyncJobList::Submit()
{
    JobManager->SubmitJobList(this);
}

void AAsyncJobManager::SubmitJobList(AAsyncJobList* InJobList)
{
    if (!InJobList->NumPendingJobs)
    {
        return;
    }

    SAsyncJob* headJob = &InJobList->JobPool[InJobList->JobPool.Size() - InJobList->NumPendingJobs];
    HK_ASSERT(headJob->Next == nullptr);

    // lock section
    {
        AMutexGurad syncGuard(InJobList->SubmitSync);

        headJob->Next            = InJobList->SubmittedJobs;
        InJobList->SubmittedJobs = InJobList->JobList;

        InJobList->SubmittedJobsCount.Add(InJobList->NumPendingJobs);
        InJobList->FetchCount.Add(InJobList->NumPendingJobs);

        TotalJobs.Add(InJobList->NumPendingJobs);

        InJobList->bSignalled = false;
    }

    NotifyThreads();
    InJobList->JobList        = nullptr;
    InJobList->NumPendingJobs = 0;
}

void AAsyncJobList::Wait()
{
    int jobsCount = JobPool.Size() - NumPendingJobs;

    if (jobsCount > 0)
    {
        while (!bSignalled)
        {
            EventDone.Wait();
        }

        HK_ASSERT(SubmittedJobsCount.Load() == 0);
        HK_ASSERT(FetchCount.Load() == 0);
        HK_ASSERT(SubmittedJobs == nullptr);

        if (NumPendingJobs > 0)
        {
            LOG("Warning: AAsyncJobList::Wait: NumPendingJobs > 0\n");

            JobPool.RemoveRange(0, jobsCount);

            JobList = JobPool.ToPtr() + size_t(NumPendingJobs - 1);
            for (int i = 1; i < NumPendingJobs; i++)
            {
                JobPool[i].Next = &JobPool[i - 1];
            }
        }
        else
        {
            JobPool.Clear();
        }
    }
}

void AAsyncJobList::SubmitAndWait()
{
    Submit();
    Wait();
}

//void FirstJob( void * _Data ) {
//    for ( int i = 0 ; i < 32 ; i++ ) {
//        GLogger.Printf( "FirstJob: Processing %d (%d) th %d\n", (size_t)_Data&0xf, i, (size_t)_Data>>16 );
//        AThread::WaitMilliseconds(1);
//    }
//}

//void SecondJob( void * _Data ) {
//    for ( int i = 0 ; i < 32 ; i++ ) {
//        GLogger.Printf( "SecondJob: Processing %d (%d) th %d\n", (size_t)_Data&0xf, i, (size_t)_Data>>16 );
//        AThread::WaitMilliseconds(1);
//    }
//}

//void AsyncJobManagerTest() {

//    AAsyncJobManager jobManager;

//    jobManager.Initialize( 4, 2 );

//    AAsyncJobList * first = jobManager.GetAsyncJobList( 0 );
//    AAsyncJobList * second = jobManager.GetAsyncJobList( 1 );

//    first->AddJob( FirstJob, ( void * )0 );
//    first->AddJob( FirstJob, ( void * )1 );

//    second->AddJob( SecondJob, ( void * )0 );
//    second->AddJob( SecondJob, ( void * )1 );

//    first->Submit();
//    second->Submit();

//    first->Wait();
//    second->Wait();

//    jobManager.Deinitialize();
//}
