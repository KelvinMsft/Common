#pragma once
#include <fltKernel.h>  

typedef struct _FIRE_GET_PCI_CONFIG_DATA
{
	ULONG BusNumber;
	ULONG DeviceNumber;
	ULONG FunctionNumber;
}FIRE_GET_PCI_CONFIG_DATA, *LPFIRE_GET_PCI_CONFIG_DATA;


VOID GetPciInfoTest();