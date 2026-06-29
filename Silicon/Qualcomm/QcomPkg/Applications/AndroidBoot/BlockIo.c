#include <Library/MemoryAllocationLib.h>
#include <Library/SlotLib.h>
#include <Protocol/BlockIo.h>
#include <Protocol/PartitionInfo.h>

#include "AndroidBoot.h"

STATIC
EFI_STATUS
GetBlockIoByPartName (
  IN CONST CHAR16 *PartName,
  OUT EFI_BLOCK_IO_PROTOCOL **BlockIo)
{
  EFI_STATUS  Status;
  EFI_HANDLE *Handles;
  UINTN       HandleCount;

  if (PartName == NULL || BlockIo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *BlockIo = NULL;

  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiBlockIoProtocolGuid, NULL, &HandleCount, &Handles);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (UINTN i = 0; i < HandleCount; i++) {
    EFI_BLOCK_IO_PROTOCOL       *BlkIo;
    EFI_PARTITION_INFO_PROTOCOL *PartInfo;

    Status = gBS->HandleProtocol (Handles[i], &gEfiBlockIoProtocolGuid, (VOID **)&BlkIo);
    if (EFI_ERROR (Status)) {
      continue;
    }

    Status = gBS->HandleProtocol (Handles[i], &gEfiPartitionInfoProtocolGuid, (VOID **)&PartInfo);
    if (EFI_ERROR (Status)) {
      continue;
    }

    if (StrCmp (PartInfo->Info.Gpt.PartitionName, PartName) == 0) {
      *BlockIo = BlkIo;
      break;
    }
  }

  FreePool (Handles);

  return *BlockIo == NULL ? EFI_NOT_FOUND : EFI_SUCCESS;
}

STATIC
EFI_STATUS
GetBlockIoByPartNameMultiSlot (
  IN CONST CHAR16 *PartName,
  OUT EFI_BLOCK_IO_PROTOCOL **BlockIo)
{
  EFI_STATUS Status;
  CHAR16     SlotPartName[36];
  CHAR16     Suffix[3];

  Status = GetBlockIoByPartName (PartName, BlockIo);
  if (Status == EFI_NOT_FOUND && HasMultipleSlots ()) {
    StrCpyS(SlotPartName, ARRAY_SIZE(SlotPartName), PartName);
    Suffix[0] = L'_';
    Suffix[1] = GetActiveSlot () == SlotA ? L'a' : L'b';
    Suffix[2] = L'\0';
    StrnCatS (SlotPartName, ARRAY_SIZE(SlotPartName), Suffix, ARRAY_SIZE(Suffix));

    Status = GetBlockIoByPartName (SlotPartName, BlockIo);
  }

  return Status;
}

EFI_STATUS
ReadPartition (
  IN CONST CHAR16 *PartName,
  OUT VOID       **Buffer,
  OUT UINTN       *BufferSize)
{
  EFI_STATUS             Status;
  EFI_BLOCK_IO_PROTOCOL *BlockIo;
  VOID                  *Data;
  UINTN                  Size;

  if (PartName == NULL || Buffer == NULL || BufferSize == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetBlockIoByPartNameMultiSlot (PartName, &BlockIo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Size = (BlockIo->Media->LastBlock + 1) * BlockIo->Media->BlockSize;
  Data = AllocatePages (EFI_SIZE_TO_PAGES (Size));
  if (Data == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = BlockIo->ReadBlocks (BlockIo, BlockIo->Media->MediaId, 0, Size, Data);
  if (EFI_ERROR (Status)) {
    FreePages (Buffer, EFI_SIZE_TO_PAGES (Size));
    return Status;
  }

  *Buffer = Data;
  *BufferSize = Size;

  return Status;
}

EFI_STATUS
WritePartition (
  IN CONST CHAR16 *PartName,
  IN VOID         *Buffer,
  IN UINTN         BufferSize)
{
  EFI_STATUS             Status;
  EFI_BLOCK_IO_PROTOCOL *BlockIo;

  if (PartName == NULL || Buffer == NULL || BufferSize == 0) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetBlockIoByPartNameMultiSlot (PartName, &BlockIo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (BlockIo->Media->ReadOnly) {
    return EFI_WRITE_PROTECTED;
  }

  BufferSize = ALIGN_VALUE(BufferSize, BlockIo->Media->BlockSize);

  return BlockIo->WriteBlocks (BlockIo, BlockIo->Media->MediaId, 0, BufferSize, Buffer);
}