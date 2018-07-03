#include <fltKernel.h>   
#include "Util.h"
#include <intrin.h>
extern "C"
{
	////////////////////////////////////////////////////////////////////
	////	Macro
	////
	#define KTIMER_TABLE_ENTRY_MAX 256
	#define p2dq(x)  (*((ULONG_PTR*)x))
	#define MSR_GS_KERNEL_BASE 0xC0000101

	////////////////////////////////////////////////////////////////////
	////	Types
	////
	typedef struct _KTIMER_TABLE_ENTRY
	{
		ULONG_PTR   Lock;
		LIST_ENTRY  Entry;
		ULONG_PTR   Time;
	}KTIMER_TABLE_ENTRY, *PKTIMER_TABLE_ENTRY; 

	typedef struct _KTIMER_TABLE
	{
		ULONG_PTR           TimerExpiry[64];
		KTIMER_TABLE_ENTRY  TimerEntries[KTIMER_TABLE_ENTRY_MAX];
	}KTIMER_TABLE, *PKTIMER_TABLE;

	////////////////////////////////////////////////////////////////////
	////	Global Variables
	////
	ULONG_PTR       g_KiProcessorBlock[16] = { 0 };
	ULONG_PTR       g_OffsetKTimerTable = 0;
	ULONG_PTR       g_KiWaitNever = 0;
	ULONG_PTR       g_KiWaitAlways = 0;
	int             g_DpcTimerCount = 0;

	KTIMER 			g_bgchckTimer = { 0 };
	KDPC			g_bgchkDPCP = { 0 };



	NTKERNELAPI UCHAR *NTAPI PsGetProcessImageFileName(_In_ PEPROCESS process);

	////////////////////////////////////////////////////////////////////
	////	Implementation
	//// 
	//------------------------------------------------------------------
	NTSTATUS DpcInfoDecrpytion(
		_In_	PKTIMER pTimer
	)
	{
		ULONG_PTR   pDpc = NULL;
		NTSTATUS    status = STATUS_SUCCESS; 
		KDPC*       DecDpc = NULL;
		int         nShift = 0;  
		void*		modBase = NULL;  
		if (!pTimer || !g_KiWaitNever || !g_KiWaitAlways)
		{
			return status;
		}

		nShift = (p2dq(g_KiWaitNever) & 0xFF);
		pDpc = (ULONG_PTR)pTimer->Dpc;

		//v19 = KiWaitNever ^ v18;
		pDpc ^= p2dq(g_KiWaitNever);

		//v18 = __ROR8__((unsigned __int64)Timer ^ _RBX, KiWaitNever);
		pDpc = _rotl64(pDpc, nShift);
		 
		pDpc ^= (ULONG_PTR)pTimer;
		
		//__asm { bswap   rbx }
		pDpc = _byteswap_uint64(pDpc);

		//_RBX = (unsigned __int64)DPC ^ KiWaitAlways; 
		pDpc ^= p2dq(g_KiWaitAlways); 

		if (!MmIsAddressValid((PVOID)pDpc))
		{
			return status;
		} 

		g_DpcTimerCount++;
		DecDpc = (KDPC*)pDpc;

		modBase = UtilPcToFileHeader(DecDpc->DeferredRoutine); 
		COMMON_DEBUG_INFO("[dpc]dpc:%p, routine:%p, modBase= %p\n", DecDpc, DecDpc->DeferredRoutine, modBase);
		return status;
	}
	//------------------------------------------------------------------
	void DpcTimerEnumeration()
	{
		PKTIMER_TABLE       pKTimerTable = 0; 
		PKTIMER             pTimer = NULL;
		PLIST_ENTRY         pListEntry = NULL;
		PLIST_ENTRY         pListEntryHead = NULL;
		int                 i = 0; 

		pKTimerTable = (PKTIMER_TABLE)(g_KiProcessorBlock[KeGetCurrentProcessorNumber()] + g_OffsetKTimerTable);
		KdPrint(("ProcNum= %d ptrKTimerTable:%p\n", KeGetCurrentProcessorNumber(), pKTimerTable));

		for (i = 0; i < KTIMER_TABLE_ENTRY_MAX; i++)
		{
			pListEntryHead = &(pKTimerTable->TimerEntries[i].Entry);
			for (pListEntry = pListEntryHead->Flink; pListEntry != pListEntryHead; pListEntry = pListEntry->Flink)
			{
				pTimer = CONTAINING_RECORD(pListEntry, KTIMER, TimerListEntry);
				if (!MmIsAddressValid(pTimer))
					continue;

				if (!pTimer->Dpc)
					continue;

				DpcInfoDecrpytion(pTimer);
			}
		}
		COMMON_DEBUG_INFO("ProcNum= %d TotalDPC:%d\n", KeGetCurrentProcessorNumber(), g_DpcTimerCount);
	} 
	//------------------------------------------------------------------
	NTSTATUS InitialSignature(void* param)
	{
		UNREFERENCED_PARAMETER(param);

		int i = 0; 
		PUCHAR StartRip = (PUCHAR)KeSetTimerEx;
		PUCHAR KiWaitNever = NULL;
		PUCHAR KiWaitAlways = NULL; 
		PUCHAR KiProcessorBlock = NULL; 
		ULONG MajorV;
		ULONG MinorV;
		ULONG BuildNO;
		UCHAR NeverSignature[3]  = { 0x48, 0x8B, 0x05 };
		UCHAR AlwaysSignature[3] = { 0x48, 0x8B, 0x1D };
		UCHAR AlwaysSignature2[3] = { 0x48, 0x8B, 0x3D };
		PsGetVersion(&MajorV, &MinorV, &BuildNO, NULL);

		if (BuildNO == 7600)
		{
			g_OffsetKTimerTable = 0x2380; 
		}
		else if (BuildNO == 7601)
		{
			g_OffsetKTimerTable = 0x2B80;
		}
		else if (BuildNO == 9200)
		{
			g_OffsetKTimerTable = 0x2f80;
			AlwaysSignature[2] = 0x3D;
		}
		else if (BuildNO == 9600)
		{
			g_OffsetKTimerTable = 0x2f80;
		}
		else if (BuildNO == 14393)
		{
			g_OffsetKTimerTable = 0x3780;
		}

		for (i = 0; i < 100 && BuildNO < 9600 ; i++)
		{
			if (StartRip[i] == 0xE8)
			{
				StartRip = StartRip + *(PLONG)(StartRip + i + 1) + 5;
			}
		}
		COMMON_DEBUG_INFO("KiSetTimerEx= %p \r\n", StartRip);

		for (i = 0; i < 100; i++)
		{
			if (StartRip[i]		== NeverSignature[0] && 
				StartRip[i + 1] == NeverSignature[1] &&
				StartRip[i + 2] == NeverSignature[2])
			{
				KiWaitNever = (PUCHAR)(((ULONG_PTR)&StartRip[i]) + *((PLONG)(StartRip+i+3)) + 7);
				COMMON_DEBUG_INFO("base= %p + offset= %p = %p \r\n", ((ULONG_PTR)&StartRip[i]), *((PLONG)(StartRip + i + 3)), KiWaitNever);
			}
			if ((StartRip[i]	== AlwaysSignature[0] && StartRip[i + 1] == AlwaysSignature[1] && StartRip[i + 2] == AlwaysSignature[2]) ||
				(StartRip[i] == AlwaysSignature2[0] && StartRip[i + 1] == AlwaysSignature2[1] && StartRip[i + 2] == AlwaysSignature2[2]))
			{
				KiWaitAlways = (PUCHAR)(((ULONG_PTR)&StartRip[i]) + *((PLONG)(StartRip+i+3)) + 7);
				COMMON_DEBUG_INFO("base= %p + offset= %p = %p \r\n", ((ULONG_PTR)&StartRip[i]), *((PLONG)(StartRip + i + 3)), KiWaitAlways);
			}
		}

		KiProcessorBlock = (PUCHAR)__readmsr(MSR_GS_KERNEL_BASE);
		  
		g_KiProcessorBlock[KeGetCurrentProcessorNumber()] = (ULONG_PTR)KiProcessorBlock; 
		g_KiWaitNever		= (ULONG_PTR)KiWaitNever;
		g_KiWaitAlways		= (ULONG_PTR)KiWaitAlways;
		 
	  
		COMMON_DEBUG_INFO("``KiWaitNever= %p KiWaitAlways= %p kpcrb= %p \r\n", g_KiWaitNever, g_KiWaitAlways, g_KiProcessorBlock);

		DpcTimerEnumeration();
		return STATUS_SUCCESS;
	}
	//--------------------------------------------------------------------------------------------// 
	VOID DpcCallback(
		IN PKDPC Dpc,
		IN PVOID DeferredContext,
		IN PVOID sys1,
		IN PVOID sys2)
	{
		UNREFERENCED_PARAMETER(Dpc);
		UNREFERENCED_PARAMETER(DeferredContext);
		UNREFERENCED_PARAMETER(sys1);
		UNREFERENCED_PARAMETER(sys2);
		LARGE_INTEGER timeout;
		timeout.QuadPart = -(1 * 10000000);
		COMMON_DEBUG_INFO("Test Dpc....\r\n");
		KeSetTimer(&g_bgchckTimer, timeout , &g_bgchkDPCP);
		return;
	}

	//------------------------------------------------------------------
	void InitDpcStructure()
	{
		LARGE_INTEGER timeout;
		timeout.QuadPart = -(1 * 1000000000);
		KeInitializeTimer(&g_bgchckTimer);
		KeInitializeDpc(&g_bgchkDPCP, DpcCallback, NULL);
		KeSetTimer(&g_bgchckTimer, timeout, &g_bgchkDPCP);
		UtilForEachProcessor(InitialSignature, NULL); 
		return;
	}
	
	//------------------------------------------------------------------
	void UninitDpcStructure()
	{  
		KeCancelTimer(&g_bgchckTimer);
		KeFlushQueuedDpcs(); 
		return;
	}
	//------------------------------------------------------------------
}
