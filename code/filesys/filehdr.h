// filehdr.h
//	Data structures for managing a disk file header.
//
//	A file header describes where on disk to find the data in a file,
//	along with other information about the file (for instance, its
//	length, owner, etc.)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef FILEHDR_H
#define FILEHDR_H

#include "disk.h"
#include "pbitmap.h"

#define NumDirect ((SectorSize - 3 * sizeof(int)) / sizeof(int))
#define OneLevelMaxFileSize (NumDirect * SectorSize) // 4KB
#define TwoLevelMaxFileSize (NumDirect * NumDirect * SectorSize) // 128KB
#define ThreeLevelMaxFileSize (NumDirect * NumDirect * NumDirect * SectorSize) // 4MB
#define FourLevelMaxFileSize (NumDirect * NumDirect * NumDirect * NumDirect * SectorSize) // 128MB
#define MaxFileSize (NumDirect * SectorSize)

// The following class defines the Nachos "file header" (in UNIX terms,
// the "i-node"), describing where on disk to find all of the data in the file.
// The file header is organized as a simple table of pointers to
// data blocks.
//
// The file header data structure can be stored in memory or on disk.
// When it is on disk, it is stored in a single sector -- this means
// that we assume the size of this data structure to be the same
// as one disk sector.  Without indirect addressing, this
// limits the maximum file length to just under 4K bytes.
//
// There is no constructor; rather the file header can be initialized
// by allocating blocks for the file (if it is a new file), or by
// reading it from disk.

class FileHeader
{
public:
	// MP4 mod tag
	FileHeader(); // dummy constructor to keep valgrind happy
	~FileHeader();

	// DEPRECATED: Initialize a file header, including allocating space on disk for the file data
	bool Allocate(PersistentBitmap *bitMap, int fileSize);

	// Initialize multi-level file header
	bool AllocateMultiLevel(PersistentBitmap *bitMap, int fileSize);

	// Return how many sectors does the level need by given fileSize
	int GetSectorNeedsByLevel(int level);

	// FindAndSet recursively
	void RecursivelyAllocate(PersistentBitmap* bitmap, bool isRightMost);

	// DEPRECATED: De-allocate this file's data blocks
	void Deallocate(PersistentBitmap *bitMap);

	// De-allocate the file's data blocks
	void DeallocateMultiLevel(PersistentBitmap *bitMap);

	// Initialize file header from disk
	void FetchFrom(int sectorNumber);

	// Write modifications to file header back to disk
	void WriteBack(int sectorNumber);

	// Convert a byte offset into the file to the disk sector containing the byte
	int ByteToSector(int offset);

	// Return the length of the file in bytes
	int FileLength();

	// Print the contents of the file.
	void Print();

private:
	/*
		MP4 hint:
		You will need a data structure to store more information in a header.
		Fields in a class can be separated into disk part and in-core part.
		Disk part are data that will be written into disk.
		In-core part are data only lies in memory, and are used to maintain the data structure of this class.
		In order to implement a data structure, you will need to add some "in-core" data
		to maintain data structure.
		
		Disk Part - numBytes, numSectors, dataSectors occupy exactly 128 bytes and will be
		written to a sector on disk.
		In-core part - none
		
	*/

	int numBytes;				// Number of bytes in the file
	int numSectors;				// Number of data sectors in the file
	int level; // Level 1 -> data, Level 2 -> Level 1
	int dataSectors[NumDirect]; // Disk sector numbers for each data
								// block in the file
};

#endif // FILEHDR_H
