// addrspace.h
//	Data structures to keep track of executing user programs
//	(address spaces).
//
//	For now, we don't keep any information about address spaces.
//	The user level CPU state is saved and restored in the thread
//	executing the user program (see thread.h).
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

// addrspace.h
#ifndef ADDRSPACE_H
#define ADDRSPACE_H

#include "copyright.h"
#include "filesys.h"

#define UserStackSize 1024

class AddrSpace {
   public:
    AddrSpace();
    AddrSpace(char *fileName);
    ~AddrSpace();

    bool isLoaded() { return pageTable != NULL; }

    void Execute();

    void SaveState();
    void RestoreState();

    ExceptionType Translate(unsigned int vaddr, unsigned int *paddr, int mode);

    // TLB helpers
    int NumPages() const { return numPages; }
    TranslationEntry *GetPageTable() { return pageTable; }
    TranslationEntry *FindPTE(int vpn);
    void SaveTLBState();
    void ClearTLB();

   private:
    TranslationEntry *pageTable;
    unsigned int numPages;

    // --- DEMAND PAGING: keep executable open for lazy page loading ---
    OpenFile *executableFile;
    int codeOffset;
    int codeSize;
    int codeVirtualAddr;
    int dataOffset;
    int dataSize;
    int dataVirtualAddr;
    int uninitOffset;
    int uninitSize;
    int uninitVirtualAddr;
#ifdef RDATA
    int rdataOffset;
    int rdataSize;
    int rdataVirtualAddr;
#endif
    // -----------------------------------------------------------------

    void InitRegisters();
};

#endif  // ADDRSPACE_H
