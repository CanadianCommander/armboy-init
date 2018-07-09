#include "config.h"
#include "processManager/processManager.h"
#include "processManager/syscallDispatcher.h"
#include "memoryManager/memoryManager.h"
#include "flashManager/flashManager.h"
#include "FAT32/fsSysCalls.h"
#include "FAT32/fat32.h"

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define DISPLAY_MOD_ID  1
#define INPUT_MOD_ID    2
#define FS_MOD_ID       3

static bool readBytesHelper(uint8_t * buff, uint32_t count, FileDescriptor * fd){
  ReadBytesArgs rbArgs;
  memset(&rbArgs, 0, sizeof(ReadBytesArgs));

  rbArgs.fd = fd;
  rbArgs.count = count;
  rbArgs.buffer = buff;
  doModuleCall(FS_MOD_ID, 4, (uint32_t)&rbArgs);
  return rbArgs.res;
}

static bool seekHelper(uint32_t offset, uint8_t reference, FileDescriptor * fd){
  SeekArgs sArgs;
  sArgs.fd = fd;
  sArgs.offset = offset;
  sArgs.reference = reference;
  doModuleCall(FS_MOD_ID, 5, (uint32_t)&sArgs);
}

#define PROGRAM_HEADER_SIZE 40
static void populateHeader(struct FlashHeader * fh, FileDescriptor * fd){
  uint8_t buff[PROGRAM_HEADER_SIZE];

  readBytesHelper(buff, PROGRAM_HEADER_SIZE, fd);
  memset(fh, 0 , sizeof(struct FlashHeader));

  uint32_t * ptr = (uint32_t*)buff;
  fh->text_start    = *ptr++;
  fh->text_end      = *ptr++;
  fh->got_start     = *ptr++;
  fh->got_end       = *ptr++;
  fh->data_start    = *ptr++;
  fh->data_end      = *ptr++;
  fh->bss_start     = *ptr++;
  fh->bss_end       = *ptr++;
  fh->reqHeapSize   = *ptr++;
  fh->reqStackSize  = *ptr++;

}

static void loadBootProgram(FileDescriptor * bProg){
  struct FlashHeader progHeader;

  populateHeader(&progHeader, bProg);
  seekHelper(0, SEEK_START, bProg);

  struct MemoryHandle * mh = requestMemory(bProg->fileSize + WORD +
                                           progHeader.bss_end - progHeader.bss_start   +
                                           progHeader.reqHeapSize + progHeader.reqStackSize + WORD,
                                           bProg->fileSize + progHeader.bss_end - progHeader.bss_start, KERNEL_PID);
  if(mh){
    uint8_t * mPtr = mh->memptr;
    if((uint32_t)mPtr % WORD != 0)mPtr+= WORD - ((uint32_t)mPtr % WORD);

    readBytesHelper(mPtr, bProg->fileSize, bProg);// write binary
    memset(mPtr + bProg->fileSize, 0, progHeader.bss_end - progHeader.bss_start);//clear bss

    struct ProcessDescriptor * pd = loadProcess(mPtr, false, PROC_TYPE_USER);
    if(pd){
      transferMemory(mh,pd->pid);
    }
    else{
      printf("Error loading process\n");
    }
  }
  else{
    printf("Error allocating memory for process\n");
  }
}

static void loadAndInitModules(){
  loadKernelModule(FS_MOD_ID);
  loadKernelModule(DISPLAY_MOD_ID);
  loadKernelModule(INPUT_MOD_ID);

  //display init
  doModuleCall(DISPLAY_MOD_ID, 1, 0);

  //user input int
  doModuleCall(INPUT_MOD_ID, 1, 0);

  //file system init
  doModuleCall(FS_MOD_ID, 0, 0);
  doModuleCall(FS_MOD_ID, 1, 0);
}


void bootstraper(void * arg){
  loadAndInitModules();

  FileDescriptor bootProgram;
  GetFileArgs gfArgs;

  memset(&gfArgs, 0, sizeof(GetFileArgs));
  gfArgs.fd = &bootProgram;
  gfArgs.path = "/boot.bin";

  doModuleCall(FS_MOD_ID, 3, (uint32_t)&gfArgs);
  if(gfArgs.res){
    loadBootProgram(&bootProgram);
  }
}
