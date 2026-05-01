// addrspace.cc
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -n -T 0 option
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you are using the "stub" file system, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "main.h"
#include "addrspace.h"
#include "machine.h"
#include "noff.h"
#include "synch.h"

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void SwapHeader(NoffHeader *noffH) {
    noffH->noffMagic = WordToHost(noffH->noffMagic);
    noffH->code.size = WordToHost(noffH->code.size);
    noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
    noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
#ifdef RDATA
    noffH->readonlyData.size = WordToHost(noffH->readonlyData.size);
    noffH->readonlyData.virtualAddr =
        WordToHost(noffH->readonlyData.virtualAddr);
    noffH->readonlyData.inFileAddr = WordToHost(noffH->readonlyData.inFileAddr);
#endif
    noffH->initData.size = WordToHost(noffH->initData.size);
    noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
    noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
    noffH->uninitData.size = WordToHost(noffH->uninitData.size);
    noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
    noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);

#ifdef RDATA
    DEBUG(dbgAddr, "code = " << noffH->code.size
                             << " readonly = " << noffH->readonlyData.size
                             << " init = " << noffH->initData.size
                             << " uninit = " << noffH->uninitData.size << "\n");
#endif
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Set up the translation from program memory to physical
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//----------------------------------------------------------------------

AddrSpace::AddrSpace() {
    #ifdef RDATA
    rdataOffset      = 0;
    rdataSize        = 0;
    rdataVirtualAddr = 0;
    #endif	
    uninitOffset      = 0;
    uninitSize        = 0;
    uninitVirtualAddr = 0;
    pageTable       = NULL;   // ADD THIS
    numPages        = 0; 	
    executableFile  = NULL;
    codeOffset      = 0;
    codeSize        = 0;
    codeVirtualAddr = 0;
    dataOffset      = 0;
    dataSize        = 0;
    dataVirtualAddr = 0;
}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Deallocate an address space.
//----------------------------------------------------------------------

AddrSpace::~AddrSpace() {
    int i;
    for (i = 0; i < (int)numPages; i++) {
        // Only free frames that were actually loaded
        if (pageTable[i].valid) {
            kernel->gPhysPageBitMap->Clear(pageTable[i].physicalPage);
        }
    }
    delete[] pageTable;

    // DEMAND PAGING: close the executable file we kept open
    if (executableFile != NULL) {
        delete executableFile;
        executableFile = NULL;
    }
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace(char *fileName)
// 	Load a user program into memory from a file.
//
//	"fileName" is the file containing the object code to load into memory
//
//  DEMAND PAGING: We no longer load all pages upfront.
//  Pages are marked invalid and loaded lazily on PageFaultException.
//----------------------------------------------------------------------

AddrSpace::AddrSpace(char *fileName) {
    OpenFile *executable = kernel->fileSystem->Open(fileName);
    NoffHeader noffH;
    unsigned int i, size;
    #ifdef RDATA
    rdataOffset      = 0;
    rdataSize        = 0;
    rdataVirtualAddr = 0;
    #endif
    uninitOffset      = 0;   // ADD
    uninitSize        = 0;   // ADD
    uninitVirtualAddr = 0;   // ADD

    pageTable       = NULL;   // ADD THIS
    numPages        = 0;      // ADD THIS/ Initialize demand paging fields to safe defaults
    executableFile  = NULL;
    codeOffset      = 0;
    codeSize        = 0;
    codeVirtualAddr = 0;
    dataOffset      = 0;
    dataSize        = 0;
    dataVirtualAddr = 0;

    if (executable == NULL) {
        DEBUG(dbgFile, "\n Error opening file.");
        return;
    }

    // Read the NOFF header
    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) &&
        (WordToHost(noffH.noffMagic) == NOFFMAGIC))
        SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

    kernel->addrLock->P();

    // How big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size
     #ifdef RDATA
     	+ noffH.readonlyData.size
     #endif
     	+ UserStackSize;
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;

    ASSERT(numPages <= NumPhysPages);

    // DEMAND PAGING: pages are loaded lazily, so we only need at least
    // one free frame to be able to serve the first fault.
    if (kernel->gPhysPageBitMap->NumClear() == 0) {
        DEBUG(dbgAddr, "No free physical frames at all");
        numPages = 0;
        delete executable;
        kernel->addrLock->V();
        return;
    }

    DEBUG(dbgAddr, "Initializing address space (demand paging): "
                       << numPages << " pages, " << size << " bytes");

    // Set up page table — all pages start INVALID (not yet in memory)
    pageTable = new TranslationEntry[numPages];
    for (i = 0; i < numPages; i++) {
        pageTable[i].virtualPage  = i;
        pageTable[i].physicalPage = -1;    // no frame assigned yet
        pageTable[i].valid        = FALSE; // triggers PageFaultException on first access
        pageTable[i].use          = FALSE;
        pageTable[i].dirty        = FALSE;
        pageTable[i].readOnly     = FALSE;
        DEBUG(dbgAddr, "Page " << i << " marked invalid (demand paging)");
    }

    // DEMAND PAGING: Save segment info so the fault handler can read the
    // correct bytes from disk when each page is first accessed.
    #ifdef RDATA
    rdataOffset      = noffH.readonlyData.inFileAddr;
    rdataSize        = noffH.readonlyData.size;
    rdataVirtualAddr = noffH.readonlyData.virtualAddr;
    #endif
    executableFile  = executable;               // Keep file open — do NOT delete here
    codeOffset      = noffH.code.inFileAddr;
    codeSize        = noffH.code.size;
    codeVirtualAddr = noffH.code.virtualAddr;
    dataOffset      = noffH.initData.inFileAddr;
    dataSize        = noffH.initData.size;
    dataVirtualAddr = noffH.initData.virtualAddr;
    uninitOffset      = noffH.uninitData.inFileAddr;
    uninitSize        = noffH.uninitData.size;
    uninitVirtualAddr = noffH.uninitData.virtualAddr;

    kernel->addrLock->V();
    // NOTE: executable is intentionally NOT deleted here
    return;
}

//----------------------------------------------------------------------
// AddrSpace::Execute
// 	Run a user program using the current thread
//----------------------------------------------------------------------

void AddrSpace::Execute() {
    kernel->currentThread->space = this;

    this->InitRegisters();  // set the initial register values
    this->RestoreState();   // load page table register

    kernel->machine->Run();  // jump to the user program

    ASSERTNOTREACHED();
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//----------------------------------------------------------------------

void AddrSpace::InitRegisters() {
    Machine *machine = kernel->machine;
    int i;

    for (i = 0; i < NumTotalRegs; i++) machine->WriteRegister(i, 0);

    machine->WriteRegister(PCReg, 0);
    machine->WriteRegister(NextPCReg, 4);
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG(dbgAddr, "Initializing stack pointer: " << numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
//----------------------------------------------------------------------

void AddrSpace::SaveState() {
#ifdef USE_TLB
    SaveTLBState();
#endif
}

void AddrSpace::RestoreState() {
#ifdef USE_TLB
    kernel->machine->pageTable     = NULL;
    kernel->machine->pageTableSize = 0;
    ClearTLB();
#else
    kernel->machine->pageTable     = pageTable;
    kernel->machine->pageTableSize = numPages;
#endif
}

TranslationEntry *AddrSpace::FindPTE(int vpn) {
    if (vpn < 0 || (unsigned int)vpn >= numPages)
        return NULL;
    return &pageTable[vpn];
}

void AddrSpace::SaveTLBState() {
#ifdef USE_TLB
    Machine *machine = kernel->machine;
    for (int i = 0; i < TLBSize; i++) {
        TranslationEntry &tlbEntry = machine->tlb[i];
        if (!tlbEntry.valid) continue;
        TranslationEntry *pte = FindPTE(tlbEntry.virtualPage);
        if (pte != NULL) {
            pte->use   = pte->use   || tlbEntry.use;
            pte->dirty = pte->dirty || tlbEntry.dirty;
        }
    }
#endif
}

void AddrSpace::ClearTLB() {
#ifdef USE_TLB
    for (int i = 0; i < TLBSize; i++)
        kernel->machine->tlb[i].valid = FALSE;
#endif
}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
//------------------------------------------------------------

//----------------------------------------------------------------------
// AddrSpace::Translate
//  Translate the virtual address in _vaddr_ to a physical address
//  and store the physical address in _paddr_.
//
//  DEMAND PAGING: If the page is not yet in memory (valid==FALSE),
//  allocate a frame and load the page from the executable file.
//----------------------------------------------------------------------
ExceptionType AddrSpace::Translate(unsigned int vaddr, unsigned int *paddr,
                                   int isReadWrite) {
    TranslationEntry *pte;
    int pfn;
    unsigned int vpn    = vaddr / PageSize;
    unsigned int offset = vaddr % PageSize;

    if (vpn >= numPages) {
        return AddressErrorException;
    }

    pte = &pageTable[vpn];

    // ---- DEMAND PAGING: page not yet loaded ----
    if (!pte->valid) {
        // Find a free physical frame
        int phyPage = kernel->gPhysPageBitMap->FindAndSet();
        if (phyPage == -1) {
            // No free frames available — would need page replacement here
            DEBUG(dbgAddr, "No free physical frames for vpn=" << vpn);
            return BusErrorException;
        }

        // Zero out the frame first
        bzero(&(kernel->machine->mainMemory[phyPage * PageSize]), PageSize);

        // Determine where in the file this virtual page lives
        int pageStartVA = vpn * PageSize;  // start virtual address of this page
        int fileAddr    = -1;
        int bytesToCopy = PageSize;
	
	#ifdef RDATA
        // Load readonlyData segment portion (string literals live here)
        if (rdataSize > 0 &&
            pageStartVA < (unsigned int)(rdataVirtualAddr + rdataSize) &&
            pageStartVA + PageSize > (unsigned int)rdataVirtualAddr) {
            int memOff = (pageStartVA >= (unsigned int)rdataVirtualAddr) ? 0 : rdataVirtualAddr - pageStartVA;
            int segOff = (pageStartVA >= (unsigned int)rdataVirtualAddr) ? pageStartVA - rdataVirtualAddr : 0;
            int bytes  = rdataSize - segOff;
            if (bytes > PageSize - memOff) bytes = PageSize - memOff;
            if (bytes > 0)
                executableFile->ReadAt(
                    &(kernel->machine->mainMemory[phyPage * PageSize + memOff]),
                    bytes, rdataOffset + segOff);
        }
	#endif
        // Load code segment portion of this page
        if (codeSize > 0 &&
            pageStartVA < (unsigned int)(codeVirtualAddr + codeSize) &&
            pageStartVA + PageSize > (unsigned int)codeVirtualAddr) {
            int memOff  = (pageStartVA >= (unsigned int)codeVirtualAddr) ? 0 : codeVirtualAddr - pageStartVA;
            int segOff  = (pageStartVA >= (unsigned int)codeVirtualAddr) ? pageStartVA - codeVirtualAddr : 0;
            int bytes   = codeSize - segOff;
            if (bytes > PageSize - memOff) bytes = PageSize - memOff;
            if (bytes > 0)
                executableFile->ReadAt(
                    &(kernel->machine->mainMemory[phyPage * PageSize + memOff]),
                    bytes, codeOffset + segOff);
        }

        // Load initData segment portion of this page
        if (dataSize > 0 &&
            pageStartVA < (unsigned int)(dataVirtualAddr + dataSize) &&
            pageStartVA + PageSize > (unsigned int)dataVirtualAddr) {
            int memOff  = (pageStartVA >= (unsigned int)dataVirtualAddr) ? 0 : dataVirtualAddr - pageStartVA;
            int segOff  = (pageStartVA >= (unsigned int)dataVirtualAddr) ? pageStartVA - dataVirtualAddr : 0;
            int bytes   = dataSize - segOff;
            if (bytes > PageSize - memOff) bytes = PageSize - memOff;
            if (bytes > 0)
                executableFile->ReadAt(
                    &(kernel->machine->mainMemory[phyPage * PageSize + memOff]),
                    bytes, dataOffset + segOff);
        }

        // Load uninitData segment portion of this page (contains string literals)
        if (uninitSize > 0 &&
            pageStartVA < (unsigned int)(uninitVirtualAddr + uninitSize) &&
            pageStartVA + PageSize > (unsigned int)uninitVirtualAddr) {
            int memOff  = (pageStartVA >= (unsigned int)uninitVirtualAddr) ? 0 : uninitVirtualAddr - pageStartVA;
            int segOff  = (pageStartVA >= (unsigned int)uninitVirtualAddr) ? pageStartVA - uninitVirtualAddr : 0;
            int bytes   = uninitSize - segOff;
            if (bytes > PageSize - memOff) bytes = PageSize - memOff;
            if (bytes > 0)
                executableFile->ReadAt(
                    &(kernel->machine->mainMemory[phyPage * PageSize + memOff]),
                    bytes, uninitOffset + segOff);
        }
       
       // Update the page table entry
        pte->physicalPage = phyPage;
        pte->valid        = TRUE;
        pte->use          = FALSE;
        pte->dirty        = FALSE;
    }
    // ---- end demand paging ----

    if (isReadWrite && pte->readOnly) {
        return ReadOnlyException;
    }

    pfn = pte->physicalPage;

    if (pfn >= NumPhysPages) {
        DEBUG(dbgAddr, "Illegal physical page " << pfn);
        return BusErrorException;
    }

    pte->use = TRUE;
    if (isReadWrite) pte->dirty = TRUE;

    *paddr = pfn * PageSize + offset;

    ASSERT((*paddr < MemorySize));

    return NoException;
}
