// Copyright 2018 Phyronnaz

#include "VoxelThreadPool.h"
#include "VoxelPrivate.h"
#include "Stats2.h"
#include "Event.h"
#include "RunnableThread.h"
#include "ScopeLock.h"

DECLARE_DWORD_COUNTER_STAT( TEXT( "VoxelThreadPoolDummyCounter" ), STAT_VoxelThreadPoolDummyCounter, STATGROUP_ThreadPoolAsyncTasks );
DECLARE_CYCLE_STAT(TEXT("FVoxelQueuedThreadPool::AddQueuedWork"), STAT_FVoxelQueuedThreadPool_AddQueuedWork, STATGROUP_Voxel);
DECLARE_CYCLE_STAT(TEXT("FVoxelQueuedThreadPool::RetractQueuedWork"), STAT_FVoxelQueuedThreadPool_RetractQueuedWork, STATGROUP_Voxel);
DECLARE_CYCLE_STAT(TEXT("FVoxelQueuedThreadPool::RetractQueuedWork.EraseFromQueue"), STAT_FVoxelQueuedThreadPool_RetractQueuedWork_EraseFromQueue, STATGROUP_Voxel);

uint32 FVoxelQueuedThread::Run()
{
	while (!TimeToDie)
	{
		// This will force sending the stats packet from the previous frame.
		SET_DWORD_STAT(STAT_VoxelThreadPoolDummyCounter, 0);
		// We need to wait for shorter amount of time
		bool bContinueWaiting = true;
		while (bContinueWaiting)
		{
			DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FVoxelQueuedThread::Run.WaitForWork"), STAT_FQueuedThread_Run_WaitForWork, STATGROUP_Voxel);
			// Wait for some work to do
			bContinueWaiting = !DoWorkEvent->Wait(10);
		}

		IVoxelQueuedWork* LocalQueuedWork = QueuedWork;
		QueuedWork = nullptr;
		FPlatformMisc::MemoryBarrier();
		check(LocalQueuedWork || TimeToDie); // well you woke me up, where is the job or termination request?
		while (LocalQueuedWork)
		{
			// Tell the object to do the work
			LocalQueuedWork->DoThreadedWork();
			// Let the object cleanup before we remove our ref to it
			LocalQueuedWork = OwningThreadPool->ReturnToPoolOrGetNextJob(this);
		}
	}
	return 0;
}

FVoxelQueuedThread::FVoxelQueuedThread() : DoWorkEvent(nullptr)
, TimeToDie(0)
, QueuedWork(nullptr)
, OwningThreadPool(nullptr)
, Thread(nullptr)
{

}

bool FVoxelQueuedThread::Create(class FVoxelQueuedThreadPool* InPool, uint32 InStackSize /*= 0*/, EThreadPriority ThreadPriority /*= TPri_Normal*/)
{
	static int32 PoolThreadIndex = 0;
	const FString PoolThreadName = FString::Printf(TEXT("VoxelPoolThread %d"), PoolThreadIndex);
	PoolThreadIndex++;

	OwningThreadPool = InPool;
	DoWorkEvent = FPlatformProcess::GetSynchEventFromPool();
	Thread = FRunnableThread::Create(this, *PoolThreadName, InStackSize, ThreadPriority, FPlatformAffinity::GetPoolThreadMask());
	check(Thread);
	return true;
}

bool FVoxelQueuedThread::KillThread()
{
	bool bDidExitOK = true;
	// Tell the thread it needs to die
	FPlatformAtomics::InterlockedExchange(&TimeToDie, 1);
	// Trigger the thread so that it will come out of the wait state if
	// it isn't actively doing work
	DoWorkEvent->Trigger();
	// If waiting was specified, wait the amount of time. If that fails,
	// brute force kill that thread. Very bad as that might leak.
	Thread->WaitForCompletion();
	// Clean up the event
	FPlatformProcess::ReturnSynchEventToPool(DoWorkEvent);
	DoWorkEvent = nullptr;
	delete Thread;
	return bDidExitOK;
}

void FVoxelQueuedThread::DoWork(IVoxelQueuedWork* InQueuedWork)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FVoxelQueuedThread::DoWork"), STAT_FVoxelQueuedThread_DoWork, STATGROUP_ThreadPoolAsyncTasks);

	check(QueuedWork == nullptr && "Can't do more than one task at a time");
	// Tell the thread the work to be done
	QueuedWork = InQueuedWork;
	FPlatformMisc::MemoryBarrier();
	// Tell the thread to wake up and do its job
	DoWorkEvent->Trigger();
}

///////////////////////////////////////////////////////////////////////////////

FVoxelQueuedThreadPool::FVoxelQueuedThreadPool()
	: SynchQueue(nullptr)
	, TimeToDie(0)
{

}

FVoxelQueuedThreadPool::~FVoxelQueuedThreadPool()
{
	Destroy();
}

bool FVoxelQueuedThreadPool::Create(uint32 InNumQueuedThreads, uint32 StackSize /*= (32 * 1024)*/, EThreadPriority ThreadPriority /*= TPri_Normal*/)
{
	// Make sure we have synch objects
	bool bWasSuccessful = true;
	check(SynchQueue == nullptr);
	SynchQueue = new FCriticalSection();
	FScopeLock Lock(SynchQueue);
	// Presize the array so there is no extra memory allocated
	check(QueuedThreads.Num() == 0);
	QueuedThreads.Empty(InNumQueuedThreads);

	// Now create each thread and add it to the array
	for (uint32 Count = 0; Count < InNumQueuedThreads && bWasSuccessful == true; Count++)
	{
		// Create a new queued thread
		FVoxelQueuedThread* pThread = new FVoxelQueuedThread();
		// Now create the thread and add it if ok
		if (pThread->Create(this, StackSize, ThreadPriority) == true)
		{
			QueuedThreads.Add(pThread);
			AllThreads.Add(pThread);
		}
		else
		{
			// Failed to fully create so clean up
			bWasSuccessful = false;
			delete pThread;
		}
	}
	// Destroy any created threads if the full set was not successful
	if (bWasSuccessful == false)
	{
		Destroy();
	}
	return bWasSuccessful;
}

void FVoxelQueuedThreadPool::Destroy()
{
	if (SynchQueue)
	{
		{
			FScopeLock Lock(SynchQueue);
			TimeToDie = 1;
			FPlatformMisc::MemoryBarrier();
			// Clean up all queued objects
			while (!QueuedWork.empty())
			{
				auto Work = QueuedWork.top().Work;
				if (ValidQueuedWork.Contains(Work))
				{
					Work->Abandon();
				}
				QueuedWork.pop();
			}
			ValidQueuedWork.Reset();
		}
		// wait for all threads to finish up
		while (1)
		{
			{
				FScopeLock Lock(SynchQueue);
				if (AllThreads.Num() == QueuedThreads.Num())
				{
					break;
				}
			}
			FPlatformProcess::Sleep(0.0f);
		}
		// Delete all threads
		{
			FScopeLock Lock(SynchQueue);
			// Now tell each thread to die and delete those
			for (int32 Index = 0; Index < AllThreads.Num(); Index++)
			{
				AllThreads[Index]->KillThread();
				delete AllThreads[Index];
			}
			QueuedThreads.Empty();
			AllThreads.Empty();
		}
		delete SynchQueue;
		SynchQueue = nullptr;
	}
}

/*int32 FVoxelQueuedThreadPool::GetNumQueuedJobs() const
{
	// this is a estimate of the number of queued jobs. 
	// no need for thread safe lock as the queuedWork array isn't moved around in memory so unless this class is being destroyed then we don't need to worry about it
	return QueuedWork.size();
}*/

int32 FVoxelQueuedThreadPool::GetNumThreads() const
{
	return AllThreads.Num();
}

void FVoxelQueuedThreadPool::AddQueuedWork(IVoxelQueuedWork* InQueuedWork)
{
	SCOPE_CYCLE_COUNTER(STAT_FVoxelQueuedThreadPool_AddQueuedWork);

	if (TimeToDie)
	{
		InQueuedWork->Abandon();
		return;
	}
	check(InQueuedWork != nullptr);
	FVoxelQueuedThread* Thread = nullptr;
	// Check to see if a thread is available. Make sure no other threads
	// can manipulate the thread pool while we do this.
	check(SynchQueue);
	FScopeLock sl(SynchQueue);
	if (QueuedThreads.Num() > 0)
	{
		// Cycle through all available threads to make sure that stats are up to date.
		int32 Index = 0;
		// Grab that thread to use
		Thread = QueuedThreads[Index];
		// Remove it from the list so no one else grabs it
		QueuedThreads.RemoveAt(Index);
	}
	// Was there a thread ready?
	if (Thread != nullptr)
	{
		// We have a thread, so tell it to do the work
		Thread->DoWork(InQueuedWork);
	}
	else
	{
		// There were no threads available, queue the work to be done
		// as soon as one does become available
		QueuedWork.push(FVoxelQueuedWorkPtr(InQueuedWork, ValidQueuedWork));
		ValidQueuedWork.Add(InQueuedWork);
	}
}

bool FVoxelQueuedThreadPool::RetractQueuedWork(IVoxelQueuedWork* InQueuedWork)
{
	SCOPE_CYCLE_COUNTER(STAT_FVoxelQueuedThreadPool_RetractQueuedWork);

	if (TimeToDie)
	{
		return false; // no special consideration for this, refuse the retraction and let shutdown proceed
	}
	check(InQueuedWork != nullptr);
	check(SynchQueue);
	FScopeLock sl(SynchQueue);

	return !!ValidQueuedWork.Remove(InQueuedWork);
}

IVoxelQueuedWork* FVoxelQueuedThreadPool::ReturnToPoolOrGetNextJob(FVoxelQueuedThread* InQueuedThread)
{
	check(InQueuedThread != nullptr);
	IVoxelQueuedWork* Work = nullptr;
	// Check to see if there is any work to be done
	FScopeLock sl(SynchQueue);
	if (TimeToDie)
	{
		check(QueuedWork.empty());  // we better not have anything if we are dying
	}
	bool bWorkIsValid = false;
	while (!QueuedWork.empty() && !bWorkIsValid)
	{
		Work = QueuedWork.top().Work;
		QueuedWork.pop();

		bWorkIsValid = ValidQueuedWork.Contains(Work);
	}
	if (bWorkIsValid)
	{
		check(Work);
		ValidQueuedWork.Remove(Work);
	}
	else
	{
		// There was no work to be done, so add the thread to the pool
		QueuedThreads.Add(InQueuedThread);
	}
	return bWorkIsValid ? Work : nullptr;
}
