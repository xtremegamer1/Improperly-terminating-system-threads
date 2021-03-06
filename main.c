//Experiment to see if threads will leak if I return from them instead of calling PsTerminateSystemThread
#include <wdm.h>

KSTART_ROUTINE ParentThread;
KSTART_ROUTINE DaughterThread;
KMUTEX AcquireToWriteCounter;

NTSTATUS DriverEntry(
	_In_ PDRIVER_OBJECT  DriverObject,
	_In_ PUNICODE_STRING RegistryPath
)
{
	UNREFERENCED_PARAMETER(DriverObject); UNREFERENCED_PARAMETER(RegistryPath);
	HANDLE ParentThreadHandle;
	PsCreateSystemThread(&ParentThreadHandle, DELETE, 0, 0, 0, ParentThread, 0);
	return ZwClose(ParentThreadHandle);
}

#define SECONDS (__int64)-10'000'000

void ParentThread(_In_ PVOID StartContext)
{
	UNREFERENCED_PARAMETER(StartContext);
	KTIMER timer;
	LARGE_INTEGER WaitTime = {.QuadPart = 2 * SECONDS};
	KeInitializeTimer(&timer);

	KeInitializeMutex(&AcquireToWriteCounter, 0);
	KeSetTimer(&timer, WaitTime, NULL);

	while (TRUE)
	{
		HANDLE threadhandle;
		KeWaitForSingleObject(&timer, Executive, KernelMode, FALSE, 0);
		KeSetTimer(&timer, WaitTime, NULL);
		for (unsigned short i = 0; i < 1000; i++)
		{
			PsCreateSystemThread(&threadhandle, DELETE, 0, 0, 0, DaughterThread, 0);
			if (ZwClose(threadhandle) != STATUS_SUCCESS)
				KeBugCheckEx(0xBE571e, 0, 0, 0, 0); //bestie (sparkling eyes emoji)
		}
	}
}

void DaughterThread(_In_ PVOID StartContext)
{
	UNREFERENCED_PARAMETER(StartContext);
	static long long TotalNumberOfThreads;
	long long ThreadIndex;
	KeWaitForMutexObject(&AcquireToWriteCounter, Executive, KernelMode, FALSE, 0);
	ThreadIndex = TotalNumberOfThreads++;
	KeReleaseMutex(&AcquireToWriteCounter, FALSE);
	DbgPrint("Thread Index: %lld\n", ThreadIndex);
	return;
}

//Threads not terminated with PsTerminateSystemThread do not appear in task manager's thread count. They do not appear to leak paged or nonpaged pool. 
//It seems safe to terminate threads by returning, although maybe it will slowly leak something.
//Update: after running for 6 hours and opening 10 million threads paged pool appeared to grow 90MB and non paged pool 10 MB.
//Could be due to a background process allocating more paged pool, if there is a leak it is not significant enough to pay any mind to.

//CONCLUSION: PsTerminateSystemThread is not required and simply returning from thread start routine causes a miniscule, if any impact on system resources.
//However PsTerminateSystemThread is able to change the thread exit status while simply returning cannot.
