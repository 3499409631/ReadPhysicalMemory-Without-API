#include"Memory.h"

#define PageCount 64
 _PAGE PageList[PageCount];
 DirectoryTableOffset = 0x0388;
PVOID PhysicalToVirtual(ULONG64 address)
{
    PHYSICAL_ADDRESS physical;
    physical.QuadPart = address;
    return MmGetVirtualForPhysical(physical);
}



ULONG64 TransformationCR3(const pageindex,ULONG64 cr3, ULONG64 VirtualAddress)
{
    cr3 &= ~0xf;
    ULONG64 PAGE_OFFSET = VirtualAddress & ~(~0ul << 12);
    ULONG64 a = 0, b = 0, c = 0;
    ReadPhysicalAddress(pageindex,(PVOID)(cr3 + 8 * ((VirtualAddress >> 39) & (0x1ffll))), &a, sizeof(a));
    if (~a & 1) return 0;
    ReadPhysicalAddress(pageindex, (PVOID)((a & ((~0xfull << 8) & 0xfffffffffull)) + 8 * ((VirtualAddress >> 30) & (0x1ffll))), &b, sizeof(b));
    if (~b & 1) return 0;
    if (b & 0x80) return (b & (~0ull << 42 >> 12)) + (VirtualAddress & ~(~0ull << 30));
    ReadPhysicalAddress(pageindex, (PVOID)((b & ((~0xfull << 8) & 0xfffffffffull)) + 8 * ((VirtualAddress >> 21) & (0x1ffll))), &c, sizeof(c));
    if (~c & 1) return 0;
    if (c & 0x80) return (c & ((~0xfull << 8) & 0xfffffffffull)) + (VirtualAddress & ~(~0ull << 21));
    ULONG64 address = 0;
    ReadPhysicalAddress(pageindex, (PVOID)((c & ((~0xfull << 8) & 0xfffffffffull)) + 8 * ((VirtualAddress >> 12) & (0x1ffll))), &address, sizeof(address));
    address &= ((~0xfull << 8) & 0xfffffffffull);
    if (!address) return 0;
    return address + PAGE_OFFSET;
}
ULONG GetDirectoryTableOffset(void)
{
    RTL_OSVERSIONINFOW Version;
    RtlGetVersion(&Version);
    switch (Version.dwBuildNumber)
    {
    case 17763:		//1809
        return 0x0278;
        break;
    case 18363:		//1909
        return 0x0280;
        break;
    case 19041:		//2004
        return 0x0388;
        break;
    case 19569:		//20H2
        return 0x0388;
        break;
    case 20180:		//21H1
        return 0x0388;
        break;
    }
    return 0x0388;
}


PTE* MemoryGetPte(const ULONG64 address)
{
    VIRTUAL_ADDRESS virtualAddress;
    virtualAddress.Value = address;

    PTE_CR3 cr3;
    cr3.Value = __readcr3();

    PML4E* pml4 = (PML4E*)(PhysicalToVirtual(PFN_TO_PAGE(cr3.Pml4)));
    const PML4E* pml4e = (pml4 + virtualAddress.Pml4Index);
    if (!pml4e->Present)
        return 0;

    PDPTE* pdpt = (PDPTE*)(PhysicalToVirtual(PFN_TO_PAGE(pml4e->Pdpt)));
    const PDPTE* pdpte = pdpte = (pdpt + virtualAddress.PdptIndex);
    if (!pdpte->Present)
        return 0;

    // sanity check 1GB page
    if (pdpte->PageSize)
        return 0;

    PDE* pd = (PDE*)(PhysicalToVirtual(PFN_TO_PAGE(pdpte->Pd)));
    const PDE* pde = pde = (pd + virtualAddress.PdIndex);
    if (!pde->Present)
        return 0;

    // sanity check 2MB page
    if (pde->PageSize)
        return 0;

    PTE* pt = (PTE*)(PhysicalToVirtual(PFN_TO_PAGE(pde->Pt)));
    PTE* pte = (pt + virtualAddress.PtIndex);
    if (!pte->Present)
        return 0;

    return pte;
}



NTSTATUS InitMemory()
{
    DirectoryTableOffset = GetDirectoryTableOffset();

    for (UINT32 i = 0; i < 64; i++)
    {
        PHYSICAL_ADDRESS maxAddress;
        maxAddress.QuadPart = MAXULONG64;

        PageList[i].VirtualAddress = MmAllocateContiguousMemory(PAGE_SIZE, maxAddress);
        if (!PageList[i].VirtualAddress)
            return 0;

        PageList[i].PTE = MemoryGetPte((ULONG64)(PageList[i].VirtualAddress));
        if (!PageList[i].PTE)
            return 0;
    }

    return STATUS_SUCCESS;
}

void ReadPhysicalAddress(const UINT32 pageIndex, const ULONG64 targetAddress, const PVOID buffer, const SIZE_T size)
{
    const ULONG pageOffset = targetAddress % PAGE_SIZE;
    const ULONG64 pageStartPhysical = targetAddress - pageOffset;

    _PAGE* pageInfo = &PageList[pageIndex];
    const ULONG64 OldPFN = pageInfo->PTE->PFN;



        pageInfo->PTE->PFN = PAGE_TO_PFN(pageStartPhysical);


    __invlpg(pageInfo->VirtualAddress);




    

    const PVOID virtualAddress = (PVOID)(((ULONG64)(pageInfo->VirtualAddress) + pageOffset));

    memcpy(buffer, virtualAddress, size);

    pageInfo->PTE->PFN = OldPFN;
    __invlpg(pageInfo->VirtualAddress);
}

void ReadProcessMemory(PEPROCESS GameEProcess,IN  PVOID BaseAddress, OUT PVOID Buffer, IN ULONG Length)
{
    const UINT32 pageIndex = KeGetCurrentProcessorIndex();

    NTSTATUS Status = STATUS_SUCCESS;

    if (BaseAddress <= 0 || (UINT_PTR)BaseAddress > 0x7FFFFFFFFFFF || Length <= 0 || Buffer <= 0) return STATUS_UNSUCCESSFUL;

    if (GameEProcess != NULL)
    {
        ULONG64 TargetAddress = (ULONG64)BaseAddress;
        SIZE_T TargetSize = Length;
        SIZE_T read = 0;

        PUCHAR Var = (PUCHAR)GameEProcess;
        ULONG64 CR3 = *(ULONG64*)(Var + 0x28);//maybe some game ac changed this 

        if (!CR3) CR3 = *(ULONG64*)(Var + DirectoryTableOffset);


        while (TargetSize)
        {
            ULONG64 PhysicalAddress = TransformationCR3(pageIndex, CR3, TargetAddress + read);
            if (!PhysicalAddress) break;
            ULONG64 ReadSize = min(PAGE_SIZE - (PhysicalAddress & 0xfff), TargetSize);

            ReadPhysicalAddress(pageIndex,(PVOID)(PhysicalAddress), (PVOID)((UINT_PTR)Buffer + read), ReadSize);


            TargetSize -= ReadSize;

            read += ReadSize;

        }
    }
}