#include"Memory.h"

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObj, PUNICODE_STRING RegistryString)
{
	InitMemory();
	DbgPrint("[RM] DriverEntry\n");
	//for test... 
	PEPROCESS pProcess;
	PsLookupProcessByProcessId((HANDLE)0xDE0, &pProcess);

	ULONG64 buffer=0;
	ReadProcessMemory(pProcess,0x7FF71692D5A0,&buffer,8);
	
	DbgPrint("[RM] buffer %llx\n", buffer);








	return STATUS_UNSUCCESSFUL;
}

