// directory.cc
//	Routines to manage a directory of file names.
//
//	The directory is a table of fixed length entries; each
//	entry represents a single file, and contains the file name,
//	and the location of the file header on disk.  The fixed size
//	of each directory entry means that we have the restriction
//	of a fixed maximum size for file names.
//
//	The constructor initializes an empty directory of a certain size;
//	we use ReadFrom/WriteBack to fetch the contents of the directory
//	from disk, and to write back any modifications back to disk.
//
//	Also, this implementation has the restriction that the size
//	of the directory cannot expand.  In other words, once all the
//	entries in the directory are used, no more files can be created.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "directory.h"

#include "copyright.h"
#include "filehdr.h"
#include "utility.h"
#include "debug.h"

AbsolutePath::AbsolutePath(char *absolutePath) {
  // DEBUG(dbgFile, "AbsolutePath: absolutepath = " << absolutePath);
  depth = 0;
  path = new char[AbsolutePathMaxLen + 1];
  strcpy(path, absolutePath);
  // DEBUG(dbgFile, "AbsolutePath: path = " << path);
  for (int i = 0; path[i] != '\0'; i++) {
    if (path[i] == '/') depth++;
  }

  name = new char* [depth];
  for (int i = 0; i < depth; i++) {
    ASSERT(absolutePath[0] == '/'); // all absolute path should start with a '/'
    name[i] = new char[FileNameMaxLen + 1];

    int j = 1;
    for (; absolutePath[j] != '/' && absolutePath[j] != '\0'; j++)
      name[i][j - 1] = absolutePath[j];
    name[i][j - 1] = '\0';
    absolutePath = absolutePath + j;
    DEBUG(dbgFile, "AbsolutePath: i, name[i] = " << i << ' ' << name[i]);
  }
}

AbsolutePath::~AbsolutePath() {
  delete[] path;
  for (int i = 0; i < depth; i++)
    delete[] name[i];
  delete[] name;
}

int AbsolutePath::GetSector(Directory* root) {
  bool isDirectory = false;
  return root->FindByAbsolutePath(this, 0, isDirectory);
}

int AbsolutePath::GetUpperLevelSector(Directory* root, int rootSector) {
  if (depth == 1) return rootSector;

  char* upperLevelPath = new char[FileNameMaxLen + 1];
  strcpy(upperLevelPath, path);
  int i = strlen(upperLevelPath) - 1;
  while (upperLevelPath[i] != '/') i--;
  upperLevelPath[i] = '\0';
  AbsolutePath* upperLevelAbsolutePath = new AbsolutePath(upperLevelPath);
  bool isDirectory = false;
  return root->FindByAbsolutePath(upperLevelAbsolutePath, 0, isDirectory);
}

//----------------------------------------------------------------------
// Directory::Directory
// 	Initialize a directory; initially, the directory is completely
//	empty.  If the disk is being formatted, an empty directory
//	is all we need, but otherwise, we need to call FetchFrom in order
//	to initialize it from disk.
//
//	"size" is the number of entries in the directory
//----------------------------------------------------------------------

Directory::Directory(int size) {
  table = new DirectoryEntry[size];

  // MP4 mod tag
  memset(table, 0, sizeof(DirectoryEntry) * size);  // dummy operation to keep valgrind happy

  tableSize = size;
  for (int i = 0; i < tableSize; i++) {
    table[i].inUse = FALSE;
    table[i].isDirectory = FALSE;
  }
}

//----------------------------------------------------------------------
// Directory::~Directory
// 	De-allocate directory data structure.
//----------------------------------------------------------------------

Directory::~Directory() {
  delete[] table;
}

//----------------------------------------------------------------------
// Directory::FetchFrom
// 	Read the contents of the directory from disk.
//
//	"file" -- file containing the directory contents
//----------------------------------------------------------------------

void Directory::FetchFrom(OpenFile *file) {
  (void)file->ReadAt((char *)table, tableSize * sizeof(DirectoryEntry), 0);
}

//----------------------------------------------------------------------
// Directory::WriteBack
// 	Write any modifications to the directory back to disk
//
//	"file" -- file to contain the new directory contents
//----------------------------------------------------------------------

void Directory::WriteBack(OpenFile *file) {
  (void)file->WriteAt((char *)table, tableSize * sizeof(DirectoryEntry), 0);
}

//----------------------------------------------------------------------
// Directory::FindIndex
// 	Look up file name in directory, and return its location in the table of
//	directory entries.  Return -1 if the name isn't in the directory.
//
//	"name" -- the file name to look up
//----------------------------------------------------------------------

int Directory::FindIndex(char *name) {
  for (int i = 0; i < tableSize; i++)
    if (table[i].inUse && !strncmp(table[i].name, name, FileNameMaxLen))
      return i;
  return -1;  // name not in directory
}

//----------------------------------------------------------------------
// Directory::Find
// 	Look up file name in directory, and return the disk sector number
//	where the file's header is stored. Return -1 if the name isn't
//	in the directory.
//
//	"name" -- the file name to look up
//----------------------------------------------------------------------

int Directory::Find(char *name) {
  int i = FindIndex(name);

  if (i != -1)
    return table[i].sector;
  return -1;
}

int Directory::FindByAbsolutePath(AbsolutePath* absolutePath, int depth, bool& isDirectory) {
  int i = FindIndex(absolutePath->name[depth]);
  if (i != -1) {
    if (depth == absolutePath->depth - 1) {
      isDirectory = table[i].isDirectory;
      return table[i].sector;
    } else {
      Directory* dir = new Directory(NumDirEntries);
      OpenFile* dirFile = new OpenFile(table[i].sector);
      dir->FetchFrom(dirFile);
      if (debug->IsEnabled('f')) {
        DEBUG(dbgFile, "FindByAbsolutePath: ");
        dir->List();
      }
      return dir->FindByAbsolutePath(absolutePath, depth + 1, isDirectory);
    }
  }
  return -1;
}

//----------------------------------------------------------------------
// Directory::Add
// 	Add a file into the directory.  Return TRUE if successful;
//	return FALSE if the file name is already in the directory, or if
//	the directory is completely full, and has no more space for
//	additional file names.
//
//	"name" -- the name of the file being added
//	"newSector" -- the disk sector containing the added file's header
//----------------------------------------------------------------------

bool Directory::Add(char *name, int newSector) {
  if (FindIndex(name) != -1)
    return FALSE;

  for (int i = 0; i < tableSize; i++)
    if (!table[i].inUse) {
      table[i].inUse = TRUE;
      strncpy(table[i].name, name, FileNameMaxLen);
      table[i].sector = newSector;
      return TRUE;
    }
  return FALSE;  // no space.  Fix when we have extensible files.
}

bool Directory::AddByAbsolutePath(AbsolutePath* absolutePath, int depth, int newSector, bool isDirectory) {
  int i = FindIndex(absolutePath->name[depth]);
  if (i != -1) { // Name found
    if (depth == absolutePath->depth - 1) {
      return false; // Already exists
    }

    // Directory found
    Directory* dir = new Directory(NumDirEntries);
    OpenFile* dirFile = new OpenFile(table[i].sector);
    dir->FetchFrom(dirFile);
    bool ok = dir->AddByAbsolutePath(absolutePath, depth + 1, newSector, isDirectory);
    dir->WriteBack(dirFile);
    return ok;
  } else { // Name not found
    if (depth == absolutePath->depth - 1) { // File not found
      for (int j = 0; j < tableSize; j++) {
        if (!table[j].inUse) {
          table[j].inUse = true;
          table[j].isDirectory = isDirectory;
          strncpy(table[j].name, absolutePath->name[depth], FileNameMaxLen);
          table[j].sector = newSector;
          return true;
        }
      }
    }
    return false;
  }
}

//----------------------------------------------------------------------
// Directory::Remove
// 	Remove a file name from the directory.  Return TRUE if successful;
//	return FALSE if the file isn't in the directory.
//
//	"name" -- the file name to be removed
//----------------------------------------------------------------------

bool Directory::Remove(char *name) {
  int i = FindIndex(name);

  if (i == -1)
    return FALSE;  // name not in directory
  table[i].inUse = FALSE;
  return TRUE;
}

//----------------------------------------------------------------------
// Directory::List
// 	List all the file names in the directory.
//----------------------------------------------------------------------

void Directory::List() {
  for (int i = 0; i < tableSize; i++)
    if (table[i].inUse)
      printf("%s\n", table[i].name);
}

void Directory::RecursivelyList(int depth) {
  for (int i = 0; i < tableSize; i++) {
    if (table[i].inUse) { 
      // Print indent
      for (int j = 0; j < depth; j++)
        printf("  ");

      // Print file
      printf("%s %s\n", (table[i].isDirectory ? "[D]" : "[F]"), table[i].name);

      // Recursive
      if (table[i].isDirectory) {
        Directory* dir = new Directory(NumDirEntries);
        OpenFile* dirFile = new OpenFile(table[i].sector);
        dir->FetchFrom(dirFile);
        dir->RecursivelyList(depth + 1);
        delete dirFile;
        delete dir;
      }
    }
  }
}

//----------------------------------------------------------------------
// Directory::Print
// 	List all the file names in the directory, their FileHeader locations,
//	and the contents of each file.  For debugging.
//----------------------------------------------------------------------

void Directory::Print() {
  FileHeader *hdr = new FileHeader;

  printf("Directory contents:\n");
  for (int i = 0; i < tableSize; i++)
    if (table[i].inUse) {
      printf("Name: %s, Sector: %d\n", table[i].name, table[i].sector);
      hdr->FetchFrom(table[i].sector);
      hdr->Print();
    }
  printf("\n");
  delete hdr;
}
