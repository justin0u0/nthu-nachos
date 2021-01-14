/**************************************************************
 *
 * userprog/ksyscall.h
 *
 * Kernel interface for systemcalls 
 *
 * by Marcus Voelp  (c) Universitaet Karlsruhe
 *
 **************************************************************/

#ifndef __USERPROG_KSYSCALL_H__
#define __USERPROG_KSYSCALL_H__

#include "kernel.h"

#include "synchconsole.h"

void SysHalt()
{
	kernel->interrupt->Halt();
}

int SysAdd(int op1, int op2)
{
	return op1 + op2;
}

#ifdef FILESYS_STUB
int SysCreate(char *filename)
{
	// return value
	// 1: success
	// 0: failed
	return kernel->interrupt->CreateFile(filename);
}
#else // FILESYS
int SysCreate(char *fileName, int fileSize) {
	return kernel->fileSystem->Create(fileName, fileSize);
}

OpenFileId SysOpen(char *fileName) {
	return kernel->fileSystem->OpenAFile(fileName);
}

int SysWrite(char *buffer, int size, OpenFileId openFileId) {
	if (openFileId == 1 && kernel->fileSystem->currentOpenFile != NULL)
		return kernel->fileSystem->currentOpenFile->Write(buffer, size);
}

int SysRead(char *buffer, int size, OpenFileId openFileId) {
	if (openFileId == 1 && kernel->fileSystem->currentOpenFile != NULL)
		return kernel->fileSystem->currentOpenFile->Read(buffer, size);
}

int SysClose(OpenFileId openFileId) {
	return kernel->fileSystem->Close(openFileId);
}
#endif

#endif /* ! __USERPROG_KSYSCALL_H__ */
