#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseLib.h>
#include <Library/SlotLib.h>

#include <Protocol/PartitionInfo.h>

#define ATTRIBUTE_ACTIVE_SHIFT 50
#define ATTRIBUTE_ACTIVE_MASK (1ULL << ATTRIBUTE_ACTIVE_SHIFT)

STATIC BOOLEAN mHasMultipleSlots = FALSE;
STATIC SLOT mSlot = SlotUnknown;

BOOLEAN
HasMultipleSlots (
  VOID)
{
  return mHasMultipleSlots;
}

SLOT
GetActiveSlot (
  VOID)
{
  return mSlot;
}

STATIC
EFI_STATUS
GetBootPartitionsInfo (
  EFI_PARTITION_INFO_PROTOCOL **BootAPartInfo,
  EFI_PARTITION_INFO_PROTOCOL **BootBPartInfo)
{
  EFI_STATUS Status;
  EFI_HANDLE *Handles;
  UINTN HandleCount;

  if (BootAPartInfo == NULL || BootBPartInfo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiPartitionInfoProtocolGuid,
                  NULL,
                  &HandleCount,
                  &Handles
                  );
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

EFI_STATUS
EFIAPI
SlotLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status;
  EFI_PARTITION_INFO_PROTOCOL *BootAPartInfo = NULL;
  EFI_PARTITION_INFO_PROTOCOL *BootBPartInfo = NULL;

  Status = GetBootPartitionsInfo (&BootAPartInfo, &BootBPartInfo);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to Locate Clock Protocol! Status = %r\n", Status));
    return Status;
  }

  if (BootAPartInfo == NULL && BootBPartInfo == NULL) {
    mHasMultipleSlots = FALSE;
    mSlot = SlotUnknown;
  } else {
    mHasMultipleSlots = TRUE;

    if (BootAPartInfo != NULL && BootBPartInfo == NULL) {
      mSlot = SlotA;
    } else if (BootAPartInfo == NULL && BootBPartInfo != NULL) {
      mSlot = SlotB;
    } else if (BootAPartInfo->Info.Gpt.Attributes & ATTRIBUTE_ACTIVE_MASK) {
      mSlot = SlotA;
    } else if (BootBPartInfo->Info.Gpt.Attributes & ATTRIBUTE_ACTIVE_MASK) {
      mSlot = SlotB;
    }
  }

  return EFI_SUCCESS;
}