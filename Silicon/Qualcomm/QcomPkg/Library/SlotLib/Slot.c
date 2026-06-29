#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseLib.h>
#include <Library/SlotLib.h>

#include <Protocol/PartitionInfo.h>

#define ATTRIBUTE_ACTIVE_SHIFT 50
#define ATTRIBUTE_ACTIVE_MASK (1ULL << ATTRIBUTE_ACTIVE_SHIFT)

EFI_STATUS
FindBootPart (
  EFI_PARTITION_INFO_PROTOCOL **BootAPartInfo,
  EFI_PARTITION_INFO_PROTOCOL **BootBPartInfo)
{
  EFI_STATUS Status;
  EFI_HANDLE *Handles;
  UINTN HandleCount;

  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiPartitionInfoProtocolGuid, NULL, &HandleCount, &Handles);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to Locate PartitionInfo Protocol Handle! Status = %r\n", Status));
    return Status;
  }

  for (UINTN i = 0; i < HandleCount; i++) {
    EFI_PARTITION_INFO_PROTOCOL *PartInfo;

    Status = gBS->HandleProtocol (Handles[i], &gEfiPartitionInfoProtocolGuid, (VOID**)&PartInfo);
    if (EFI_ERROR (Status)) {
      continue;
    }

    if (StrCmp (PartInfo->Info.Gpt.PartitionName, L"boot_a") == 0) {
      *BootAPartInfo = PartInfo;
    } else if (StrCmp (PartInfo->Info.Gpt.PartitionName, L"boot_b") == 0) {
      *BootBPartInfo = PartInfo;
    }
  }

  FreePool (Handles);
  return Status;
}

SLOT
GetActiveSlot (VOID)
{
  EFI_STATUS Status;
  EFI_PARTITION_INFO_PROTOCOL *BootAPartInfo = NULL;
  EFI_PARTITION_INFO_PROTOCOL *BootBPartInfo = NULL;

  Status = FindBootPart(&BootAPartInfo, &BootBPartInfo);
  if (EFI_ERROR (Status) || BootAPartInfo == NULL || BootBPartInfo == NULL) {
    return SlotUnknown;
  }

  if (BootAPartInfo->Info.Gpt.Attributes & ATTRIBUTE_ACTIVE_MASK) {
    return SlotA;
  } else if (BootBPartInfo->Info.Gpt.Attributes & ATTRIBUTE_ACTIVE_MASK) {
    return SlotB;
  }

  return SlotUnknown;
}
