#include <Library/FdtLib.h>
#include <Library/ArmLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/MemoryMapHelperLib.h>
#include <Library/MemoryAllocationLib.h>

#include "AndroidBoot.h"

STATIC
EFI_STATUS
UpdateFdt (
  IN VOID  *Fdt,
  IN VOID  *Ramdisk,
  IN UINTN  RamdiskSize)
{
  INT32 Chosen;
  INT32 Err;

  Chosen = FdtPathOffset (Fdt, "/chosen");
  if (Chosen < 0) {
    Chosen = FdtAddSubnode (Fdt, 0, "chosen");
    if (Chosen < 0) {
      return EFI_INVALID_PARAMETER;
    }
  }

  Err = FdtSetPropU64 (Fdt, Chosen, "linux,initrd-start", (UINT64)(UINTN)Ramdisk);
  if (Err != 0) {
    return EFI_INVALID_PARAMETER;
  }

  Err = FdtSetPropU64 (Fdt, Chosen, "linux,initrd-end", (UINT64)(UINTN)Ramdisk + RamdiskSize);
  if (Err != 0) {
    return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}


STATIC
EFI_STATUS
ExitBootServices (
  VOID)
{
  EFI_STATUS               Status;
  UINTN                    MemoryMapSize;
  EFI_MEMORY_DESCRIPTOR   *MemoryMap = NULL;
  UINTN                    MapKey;
  UINTN                    DescriptorSize;
  UINT32                   DescriptorVersion;

  do {
    if (MemoryMap != NULL) {
      FreePool (MemoryMap);
    }

    MemoryMap = NULL;
    MemoryMapSize = 0;

    Status = gBS->GetMemoryMap (&MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
    if ((Status != EFI_BUFFER_TOO_SMALL) || (MemoryMapSize == 0)) {
      return Status;
    }

    MemoryMapSize += 64 * DescriptorSize;
    MemoryMap = AllocateZeroPool (MemoryMapSize);
    if (MemoryMap == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    Status = gBS->GetMemoryMap (&MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = gBS->ExitBootServices (gImageHandle, MapKey);
  } while (EFI_ERROR (Status));

  return EFI_SUCCESS;
}

EFI_STATUS
BootLinux (
  IN VOID  *Kernel,
  IN UINTN  KernelSize,
  IN VOID  *Ramdisk,
  IN UINTN  RamdiskSize,
  IN VOID  *Fdt)
{
  EFI_STATUS   Status;
  VOID        *NewFdt;
  UINTN        NewFdtSize;
  INT32        Err;
  LINUX_KERNEL Linux;

  NewFdtSize = ALIGN_VALUE (FdtTotalSize (Fdt) + 0x1000, 0x1000);
  NewFdt = AllocatePages (EFI_SIZE_TO_PAGES (NewFdtSize));
  if (NewFdt == NULL) {
    Print (L"Failed to allocate memory for FDT\n");
    return EFI_OUT_OF_RESOURCES;
  }

  Err = FdtOpenInto (Fdt, NewFdt, NewFdtSize);
  if (Err != 0) {
    Print (L"Failed to open FDT! Err = %d\n", Err);
    return EFI_INVALID_PARAMETER;
  }

  Status = UpdateFdt (NewFdt, Ramdisk, RamdiskSize);
  if (EFI_ERROR (Status)) {
    Print (L"Failed to update FDT! Status = %r\n", Status);
    return Status;
  }

  Status = ExitBootServices ();
  if (EFI_ERROR (Status)) {
    Print (L"Failed to exit boot services! Status = %r\n", Status);
    return Status;
  }

  ArmDisableInterrupts ();
  ArmDisableAsynchronousAbort ();

  if (KernelSize != 0) {
    WriteBackInvalidateDataCacheRange (Kernel, KernelSize);
  }

  if (Ramdisk != NULL && RamdiskSize != 0) {
    WriteBackInvalidateDataCacheRange (Ramdisk, RamdiskSize);
  }

  WriteBackInvalidateDataCacheRange (NewFdt, NewFdtSize);

  ArmCleanDataCache ();
  ArmInvalidateInstructionCache ();

  ArmDisableDataCache ();
  ArmDisableInstructionCache ();
  ArmDisableMmu ();
  ArmInvalidateTlb ();

  Linux = (LINUX_KERNEL)Kernel;
  Linux ((UINT64)(UINTN)NewFdt, 0, 0, 0);

  while (TRUE) {
  }

  return EFI_SUCCESS;
}