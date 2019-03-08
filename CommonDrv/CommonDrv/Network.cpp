extern "C"
{
#include "Network.h"  
	typedef enum _IO
	{
		MMIO = 0,
		MEMORY_ACCESS,
		NO_STATUS,
	}IO_STATUS;

#define MAC_ADDR_0_HIGH			0x00000410 /* upper 2 bytes */
#define MAC_ADDR_0_LOW			0x00000414 /* lower 4 bytes */
#define NUM_OF_BARS  6 
#define MAC_ADDR_LEN 6
#define INTEL_PHYS_ADDR_OFFSET 0x40
#define REALTEK_NIC		0x10EC
#define INTEL_NIC		0x8086
	//------------------------------------------------------------------------------------------------
	BOOLEAN GetPciConfigData(ULONG dwBusNumber, ULONG dwDeviceNumber, ULONG dwFunctionNumber, PPCI_COMMON_CONFIG pConfigData)
	{
		PCI_SLOT_NUMBER SlotNumber = { 0 };

		SlotNumber.u.bits.DeviceNumber = dwDeviceNumber;
		SlotNumber.u.bits.FunctionNumber = dwFunctionNumber;

		//KdPrint(("Input dwBusNumber: %.8X, dwDeviceNumber: %.8X, dwFunctionNumber: %.8X\n", dwBusNumber, dwDeviceNumber, dwFunctionNumber));
		if (sizeof(PCI_COMMON_CONFIG) == HalGetBusDataByOffset(PCIConfiguration, dwBusNumber, SlotNumber.u.AsULONG, pConfigData, 0, sizeof(PCI_COMMON_CONFIG)))
		{
			KdPrint(("GetPciConfigData Success!!!\n"));
			return TRUE;
		}
		else
		{ 
			return FALSE;
		}
	} 
	//------------------------------------------------------------------------------------------------ 
	void PciUnmappingDeviceMemory(
		IO_STATUS			IoStatus,
		CHAR*				MappedAddr,
		ULONG				IoSize
	)
	{
		if (IoStatus == MEMORY_ACCESS)
		{
			MmUnmapIoSpace(MappedAddr, IoSize);
			MappedAddr = NULL;
		}
	}
	//------------------------------------------------------------------------------------------------
	IO_STATUS PciMappingDeviceMemory(
		ULONG				BusNum,
		PCI_SLOT_NUMBER SlotNumber,
		ULONG			BaseAddress,
		PCHAR*				IoAddr,
		ULONG*				IoSize
	)
	{
		ULONG		dwIoSize = 0;
		CHAR*		dwIoBase = 0;
		IO_STATUS	IoStatus = NO_STATUS; 

		if (!BaseAddress)
		{
			return IoStatus;
		}

		// I/O access
		if ((BaseAddress & 0xF) == 1)
		{
			dwIoBase = (CHAR*)(BaseAddress & 0xFFF0);
			dwIoSize = 0;
			IoStatus = MMIO;
		}
		// Memory Access
		if ((BaseAddress != 0) && ((BaseAddress & 0x1) == 0))
		{
			dwIoSize = 0xFFFFFFFF;
			PHYSICAL_ADDRESS MmIoaddr = { 0x0,0x0 };
			HalSetBusDataByOffset(PCIConfiguration,
				BusNum,
				SlotNumber.u.AsULONG,
				&dwIoSize,
				FIELD_OFFSET(PCI_COMMON_CONFIG, u.type0.BaseAddresses),
				4);

			HalGetBusDataByOffset(PCIConfiguration,
				BusNum,
				SlotNumber.u.AsULONG,
				&dwIoSize,
				FIELD_OFFSET(PCI_COMMON_CONFIG, u.type0.BaseAddresses),
				4);

			HalSetBusDataByOffset(PCIConfiguration,
				BusNum,
				SlotNumber.u.AsULONG,
				&BaseAddress,
				FIELD_OFFSET(PCI_COMMON_CONFIG, u.type0.BaseAddresses),
				4);

			dwIoSize &= 0xFFFFFFF0; 
			dwIoSize = ((~dwIoSize) + 1); 
			MmIoaddr.QuadPart = BaseAddress & 0xFFFFFFFF0; 
			dwIoBase = (CHAR *)MmMapIoSpace(MmIoaddr, dwIoSize, MmNonCached); 
			if (dwIoBase)
			{
				IoStatus = MEMORY_ACCESS;
			}
		}

		if (IoSize)
		{
			*IoSize = dwIoSize;	 
		}
		if (IoAddr)
		{
			*IoAddr = dwIoBase;
		}

		return IoStatus;
	}
	//------------------------------------------------------------------------------------------------
	void PciMemcpy(CHAR* Addr, ULONG Size, ULONG Offset, CHAR* Value)
	{
		if (!Value || !Addr)
		{
			return;
		}

		if ((Addr < (char*)0xFFFF))
		{
			for (int i = 0; i < (int)Size; i++)
			{
			  Value[i] = READ_PORT_UCHAR((PUCHAR)(Addr + Offset + i));
			}
			return;
		}  
		memcpy(Value, Addr + Offset, Size); 
	}
	//------------------------------------------------------------------------------------------------
	BOOLEAN GetMacAddress(ULONG dwBusNumber, ULONG dwDeviceNumber, ULONG dwFunctionNumber, CHAR *lpMac)
	{
		PCI_SLOT_NUMBER SlotNumber = { 0 };
		PCI_COMMON_CONFIG PciConfigData = {};

		CHAR* dwIoBase = 0; 

		PHYSICAL_ADDRESS MmIoaddr = { 0x0,0x0 };

		BOOLEAN bResult = FALSE;
		ULONG PciHeaderSize = 0;

		SlotNumber.u.bits.DeviceNumber = dwDeviceNumber;
		SlotNumber.u.bits.FunctionNumber = dwFunctionNumber;
	
		PciHeaderSize = HalGetBusDataByOffset(PCIConfiguration,
							dwBusNumber,
							SlotNumber.u.AsULONG,
							&PciConfigData, 0, sizeof(PCI_COMMON_CONFIG)
						);

		KdPrint(("Input dwBusNumber: %.8X, dwDeviceNumber: %.8X, dwFunctionNumber: %.8X\n", dwBusNumber, dwDeviceNumber, dwFunctionNumber));
		if (sizeof(PCI_COMMON_CONFIG) != PciHeaderSize || 
			PciConfigData.BaseClass != PCI_CLASS_NETWORK_CTLR)
		{
			return bResult;
		}

		DbgPrint("MacPci:Network Type: %x", PciConfigData.VendorID);
		switch (PciConfigData.VendorID)
		{
			case REALTEK_NIC:
			{	//Realtek 
				IO_STATUS IoStatus = NO_STATUS;
				IoStatus = PciMappingDeviceMemory(dwBusNumber, SlotNumber, PciConfigData.u.type0.BaseAddresses[0], &dwIoBase, nullptr);
				if(IoStatus != MMIO)
				{
					break;
				}

				DbgPrint("Realtek VA= 0x%p  PA= 0x%p, size= %x \r\n", dwIoBase, PciConfigData.u.type0.BaseAddresses[0], 0);
				PciMemcpy(dwIoBase, 6 , 0 , lpMac);
		
				DbgPrint("MAC Address Is : %.2X-%.2X-%.2X-%.2X-%.2X-%.2X\n", lpMac[0], lpMac[1], lpMac[2], lpMac[3], lpMac[4], lpMac[5]);
				bResult = TRUE;
			}
			break;
			case INTEL_NIC:
			{
				DbgPrint("Intel\n"); 
				__try
				{
					IO_STATUS IoStatus[5] = { NO_STATUS };
					CHAR	  Result[NUM_OF_BARS][MAC_ADDR_LEN] = {0 };
					ULONG	  dwIoSize[6] = { 0 };
					CHAR*	  lpMmio[6] = { 0 };
					UCHAR	  Tmp[1024] = { 0 };
					UCHAR*	  TmpPtr = Tmp;
					for (int i = 0; i < NUM_OF_BARS; i++)
					{
						IoStatus[i] = PciMappingDeviceMemory(dwBusNumber, SlotNumber, PciConfigData.u.type0.BaseAddresses[i], &lpMmio[i], &dwIoSize[i]);
						if (IoStatus[i] == NO_STATUS)
						{
							continue;
						} 
				 
						PciMemcpy(lpMmio[i], 6, INTEL_PHYS_ADDR_OFFSET, &Result[i][0]);    

						PciUnmappingDeviceMemory(IoStatus[i], lpMmio[i], dwIoSize[i]);

						DbgPrint("Intel VA= 0x%p  PA= 0x%p, size= %x \r\n", lpMmio[i], PciConfigData.u.type0.BaseAddresses[i], dwIoSize[i]);
						DbgPrint("MAC Address Is : %.2X-%.2X-%.2X-%.2X-%.2X-%.2X\n", Result[i][0], Result[i][1], Result[i][2], Result[i][3], Result[i][4], Result[i][5]); 
					} 
					bResult = TRUE; 
				}
				__except (EXCEPTION_EXECUTE_HANDLER)
				{ 
				} 
 
			}
			break;
		default: 
			break; 
		} 
		return bResult;
	}
	//------------------------------------------------------------------------------------------------ 
	VOID GetPciInfoTest()
	{
		PCI_COMMON_CONFIG PciConfigData;
		ULONG BusNumber = 0;
		ULONG DeviceNumber = 0;
		ULONG FunctionNumber = 0;
		CHAR  Mac[6] = { 0 };

		for (BusNumber = 0; BusNumber < 256; BusNumber++)
		{
			for (DeviceNumber = 0; DeviceNumber < 32; DeviceNumber++)
			{
				for (FunctionNumber = 0; FunctionNumber < 8; FunctionNumber++)
				{
					if (GetPciConfigData(BusNumber, DeviceNumber, FunctionNumber, &PciConfigData))
					{
						if (PciConfigData.BaseClass == PCI_CLASS_NETWORK_CTLR)
						{
							DbgPrint("MacPci : Get %d %d %d :\n", BusNumber, DeviceNumber, FunctionNumber);

							GetMacAddress(BusNumber, DeviceNumber, FunctionNumber, Mac);
						}
					}
				}
			}
		}
	}
}