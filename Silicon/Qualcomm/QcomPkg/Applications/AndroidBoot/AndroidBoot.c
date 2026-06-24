#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/FdtLib.h>
#include <Library/UefiLib.h>
#include <Library/ArmLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/MemoryMapHelperLib.h>
#include <Library/SlotLib.h>

#include <Protocol/SimpleFileSystem.h>
#include <Protocol/BlockIo.h>
#include <Protocol/PartitionInfo.h>
#include <Protocol/EFIScm.h>

#include <Guid/FileInfo.h>

#include "tinf/tinf.h"

#define BOOT_MAGIC "ANDROID!"
#define VENDOR_BOOT_MAGIC "VNDRBOOT"

#define DTB_MAGIC 0xedfe0dd0

typedef struct {
  CHAR8  Magic[8];
  UINT32 KernelSize;
  UINT32 KernelAddress;
  UINT32 RamdiskSize;
  UINT32 RamdiskAddress;
  UINT32 SecondSize;
  UINT32 SecondAddress;
  UINT32 TagsAddress;
  UINT32 PageSize;
  UINT32 HeaderVersion;
  UINT32 OsVersion;
  UINT8  Name[16];
  UINT8  Cmdline[512];
  UINT32 Id[8];
  UINT8  ExtraCmdline[1024];
} BOOT_IMAGE_HEADER_V0;

typedef struct {
  CHAR8  Magic[8];
  UINT32 KernelSize;
  UINT32 RamdiskSize;
  UINT32 OsVersion;
  UINT32 HeaderSize;
  UINT32 Reserved[4];
  UINT32 HeaderVersion;
  UINT8  Cmdline[1536];
} BOOT_IMAGE_HEADER_V3;

typedef struct {
  CHAR8  Magic[8];
  UINT32 HeaderSize;
  UINT32 PageSize;
  UINT32 KernelAddr;
  UINT32 RamdiskAddr;
  UINT32 VendorRamdiskSize;
  UINT8  Cmdline[2048];
  UINT32 TagsAddress;
  UINT8  Name[16];
  UINT32 HeaderVersion;
  UINT32 DtbSize;
  UINT64 DtbAddr;

  // V4 params
  UINT32 VendorRamdiskTableSize;
  UINT32 VendorRamdiskTableEntryNum;
  UINT32 VendorRamdiskTableEntrySize;
  UINT32 BootConfigSize;
} VENDOR_BOOT_IMAGE_HEADER_V3;

typedef struct {
  UINT64 Code;
  UINT64 TextOffset;
  UINT64 ImageSize;
  UINT64 Flags;
  UINT64 Reserved1;
  UINT64 Reserved2;
  UINT64 Reserved3;
  UINT32 Magic;
  UINT32 Reserved4;
} KERNEL_HEADER;

typedef VOID (*LINUX_KERNEL) (UINT64 X0, UINT64 X1, UINT64 X2, UINT64 X3);

EFI_STATUS
GetRootFs (
  OUT EFI_FILE_PROTOCOL **Root)
{
  EFI_STATUS Status;
  EFI_HANDLE *Handles;
  UINTN HandleCount;

  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &HandleCount, &Handles);
  if (EFI_ERROR (Status)) {
    Print (L"Failed to Locate SimpleFS Protocol Handle! Status = %r\n", Status);
    return Status;
  }

  for (UINTN i = 0; i < HandleCount; i++) {
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Fs;
    EFI_FILE_PROTOCOL *TempRoot;
    EFI_FILE_PROTOCOL *File;

    Status = gBS->HandleProtocol (Handles[i], &gEfiSimpleFileSystemProtocolGuid, (VOID **)&Fs);
    if (EFI_ERROR (Status)) {
      continue;
    }

    Status = Fs->OpenVolume (Fs, &TempRoot);
    if (EFI_ERROR (Status)) {
      continue;
    }

    Status = TempRoot->Open (TempRoot, &File, L"boot.img", EFI_FILE_MODE_READ, 0);
    if (!EFI_ERROR (Status)) {
      *Root = TempRoot;
      File->Close (File);
      FreePool (Handles);
      return EFI_SUCCESS;
    }

    TempRoot->Close (TempRoot);
  }

  FreePool (Handles);

  return EFI_NOT_FOUND;
}

EFI_STATUS
ReadFile (
  EFI_FILE_PROTOCOL *File,
  VOID **Buffer,
  UINTN *BufferSize)
{
  EFI_STATUS Status;
  EFI_FILE_INFO *Info;
  UINTN InfoSize = 0;

  Status = File->GetInfo (File, &gEfiFileInfoGuid, &InfoSize, NULL);
  if (EFI_ERROR (Status) && Status != EFI_BUFFER_TOO_SMALL) {
    Print (L"Failed to get File Info Size! Status = %r\n", Status);
    return Status;
  }

  Info = AllocateZeroPool (InfoSize);
  if (Info == NULL) {
    Print (L"Failed to Allocate Memory for File Info!\n");
    return EFI_OUT_OF_RESOURCES;
  }

  Status = File->GetInfo (File, &gEfiFileInfoGuid, &InfoSize, Info);
  if (EFI_ERROR (Status)) {
    Print (L"Failed to get File Info! Status = %r\n", Status);
    goto Cleanup;
  }

  *BufferSize = Info->FileSize;
  *Buffer = AllocateZeroPool (*BufferSize);
  if (*Buffer == NULL) {
    Print (L"Failed to Allocate Memory for File Content!\n");
    Status = EFI_OUT_OF_RESOURCES;
    goto Cleanup;
  }

  Status = File->Read (File, BufferSize, *Buffer);
  if (EFI_ERROR (Status)) {
    Print (L"Failed to Read File Content! Status = %r\n", Status);
    goto Cleanup;
  }

Cleanup:
  if (Info != NULL) {
    FreePool (Info);
  }

  return Status;
}

EFI_STATUS
UpdateFdt (
  VOID* Fdt,
  VOID *Ramdisk,
  UINTN RamdiskSize)
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

  Err = FdtSetPropU64 (Fdt, Chosen, "linux,initrd-start", (UINT64)Ramdisk);
  if (Err) {
    return EFI_INVALID_PARAMETER;
  }

  Err = FdtSetPropU64 (Fdt, Chosen, "linux,initrd-end", (UINT64)Ramdisk + RamdiskSize);
  if (Err) {
    return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
ExitBootService ()
{
  EFI_STATUS Status;
  UINTN MemoryMapSize = 0;
  EFI_MEMORY_DESCRIPTOR *MemoryMap = NULL;
  UINTN MapKey = 0;
  UINTN DescriptorSize = 0;
  UINT32 DescriptorVersion = 0;

  do {
    if (MemoryMap != NULL) {
      FreePool (MemoryMap);
    }

    MemoryMap = NULL;
    MemoryMapSize = 0;
    Status = gBS->GetMemoryMap (&MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
    if ((Status != EFI_BUFFER_TOO_SMALL) || !MemoryMapSize) {
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
BootLinux(
  VOID *Kernel,
  UINTN KernelSize,
  VOID *Ramdisk,
  UINTN RamdiskSize,
  VOID *Fdt)
{
  EFI_STATUS Status;
  QCOM_SCM_PROTOCOL *ScmProtocol;
  VOID* NewFdt;
  UINTN NewFdtSize;
  INT32 Err;

  NewFdtSize = ALIGN_VALUE (FdtTotalSize (Fdt) + 0x1000, 0x1000);
  NewFdt = AllocatePages (EFI_SIZE_TO_PAGES(NewFdtSize));
  if (NewFdt == NULL) {
    Print (L"Failed to Allocate Memory for NewFdt\n");
    return EFI_OUT_OF_RESOURCES;
  }

  DEBUG ((EFI_D_ERROR, "FDT base: 0x%p, size: 0x%lx\n", NewFdt, NewFdtSize));

  Err = FdtOpenInto (Fdt, NewFdt, NewFdtSize);
  if (Err) {
    Print (L"FdtOpenInto is Failed! Err = %d\n", Err);
    return EFI_INVALID_PARAMETER;
  }

  Status = UpdateFdt (NewFdt, Ramdisk, RamdiskSize);
  if (EFI_ERROR (Status)) {
    Print (L"Failed to Update FDT! Status = %r\n", Status);
    return Status;
  }

  Status = gBS->LocateProtocol (&gQcomScmProtocolGuid, NULL, (VOID *)&ScmProtocol);
  if (EFI_ERROR (Status)) {
    Print (L"Failed to Locate SCM Protocol! Status = %r\n", Status);
    return Status;
  }

  Status = ScmProtocol->ScmExitBootServicesHandler (ScmProtocol);
  if (EFI_ERROR (Status)) {
    Print (L"Scm Exit Boot Services is Failed! Status = %r\n", Status);
    return Status;
  }

  Status = ExitBootService ();
  if (EFI_ERROR (Status)) {
    Print (L"Failed to Exit Boot Service! Status = %r\n", Status);
    return Status;
  }

  ArmDisableBranchPrediction ();
  ArmDisableInterrupts ();
  ArmDisableAsynchronousAbort ();

  WriteBackInvalidateDataCacheRange (Kernel, KernelSize);
  WriteBackInvalidateDataCacheRange (Ramdisk, RamdiskSize);
  WriteBackInvalidateDataCacheRange (NewFdt, NewFdtSize);

  ArmCleanDataCache ();
  ArmInvalidateInstructionCache ();

  ArmDisableDataCache ();
  ArmDisableInstructionCache ();
  ArmDisableMmu ();
  ArmInvalidateTlb ();

  LINUX_KERNEL Linux = Kernel;
  Linux ((UINT64)NewFdt, 0, 0, 0);

  while (TRUE) {}
  return EFI_SUCCESS;
}

BOOLEAN IsGzip (
  UINT8* Buf,
  UINTN Len)
{
  if (Len >= 18 && Buf && Buf[0] == 0x1f && Buf[1] == 0x8b && Buf[2] == 0x08) {
    return TRUE;
  }

  return FALSE;
}

UINT32 ReadGzipLen (
  UINT8 *Buf,
  UINTN Len)
{
  return *(UINT32*)(Buf + Len - 4);
}

EFI_STATUS
LoadBoot (
  VOID *Boot,
  VOID *VendorBoot,
  VOID *Fdt)
{
  EFI_STATUS Status;
  BOOT_IMAGE_HEADER_V0 *BootHeaderV0 = Boot;
  BOOT_IMAGE_HEADER_V3 *BootHeaderV3 = Boot;
  VENDOR_BOOT_IMAGE_HEADER_V3 *VendorBootHeader = VendorBoot;
  EFI_MEMORY_REGION_DESCRIPTOR KernelRegion;
  KERNEL_HEADER *KernelHeader;
  UINT32 HeaderVersion;
  UINT32 PageSize;
  BOOLEAN GzipKernel;
  UINTN KernelSize;
  UINTN AlignedKernelSize;
  UINTN KernelOffset;
  UINTN KernelTextOffset = 0x80000;
  UINTN RamdiskSize;
  UINTN AlignedRamdiskSize;
  UINTN RamdiskOffset;
  VOID *Kernel;
  VOID *Ramdisk;

  HeaderVersion = BootHeaderV0->HeaderVersion;
  DEBUG ((EFI_D_ERROR, "Boot image header version: %d\n", HeaderVersion));

  if (HeaderVersion >= 3) {
    PageSize = VendorBootHeader->PageSize;
    KernelSize = BootHeaderV3->KernelSize;
    AlignedKernelSize = ALIGN_VALUE (KernelSize, PageSize);
    KernelOffset = ALIGN_VALUE (BootHeaderV3->HeaderSize, PageSize);
    RamdiskSize = BootHeaderV3->RamdiskSize;
    AlignedRamdiskSize = ALIGN_VALUE (RamdiskSize + VendorBootHeader->VendorRamdiskSize, PageSize);
    RamdiskOffset = KernelOffset + ALIGN_VALUE (KernelSize, PageSize);

    if (HeaderVersion == 4) {
      AlignedRamdiskSize = ALIGN_VALUE (AlignedRamdiskSize + VendorBootHeader->BootConfigSize, PageSize);
    }
  } else {
    PageSize = BootHeaderV0->PageSize;
    KernelSize = BootHeaderV0->KernelSize;
    AlignedKernelSize = ALIGN_VALUE (KernelSize, PageSize);
    KernelOffset = ALIGN_VALUE (sizeof(BOOT_IMAGE_HEADER_V0), PageSize);
    RamdiskSize = BootHeaderV0->RamdiskSize;
    AlignedRamdiskSize = ALIGN_VALUE (RamdiskSize, PageSize);
    RamdiskOffset = KernelOffset + ALIGN_VALUE (KernelSize, PageSize);
  }

  GzipKernel = IsGzip(Boot + KernelOffset, KernelSize);
  if (GzipKernel) {
    for (UINTN i = KernelSize - sizeof(UINT32); i > 0; i--) {
      if (*(UINT32*)(Boot + KernelOffset + i) == DTB_MAGIC) {
        KernelSize = i;
      }
    }

    AlignedKernelSize = ALIGN_VALUE (ReadGzipLen(Boot + KernelOffset, KernelSize), PageSize);
  }

  KernelHeader = Boot + KernelOffset;
  if (!GzipKernel && KernelHeader->ImageSize) {
    KernelTextOffset = KernelHeader->TextOffset;
  }

  Status = LocateMemoryRegionByName ("Kernel", &KernelRegion);
  if (!EFI_ERROR (Status) && (KernelRegion.Length + KernelTextOffset) >= AlignedKernelSize) {
    Kernel = (VOID*)KernelRegion.Address;
  } else {
    Print (L"Kernel Region is too small. Trying to allocate\n");
    Kernel = AllocateAlignedPages (EFI_SIZE_TO_PAGES (AlignedKernelSize + KernelTextOffset), SIZE_2MB);
    if (Kernel == NULL) {
      Print (L"Failed to Allocate Memory for Kernel\n");
      return EFI_OUT_OF_RESOURCES;
    }
  }
  Kernel += KernelTextOffset;

  Ramdisk = AllocatePages (EFI_SIZE_TO_PAGES (AlignedRamdiskSize));
  if (Ramdisk == NULL) {
    Print (L"Failed to Allocate Memory for Ramdisk\n");
    FreePages (Kernel, EFI_SIZE_TO_PAGES (AlignedKernelSize));
    return EFI_OUT_OF_RESOURCES;
  }

  DEBUG ((EFI_D_ERROR, "Kernel base: 0x%p, size: 0x%lx\n", Kernel, KernelSize));
  DEBUG ((EFI_D_ERROR, "Ramdisk base: 0x%p, size: 0x%lx\n", Ramdisk, RamdiskSize));

  if (GzipKernel) {
    UINT32 DestLen = AlignedKernelSize;
    if (tinf_gzip_uncompress (Kernel, &DestLen, Boot + KernelOffset, KernelSize) != TINF_OK) {
      Print (L"Failed to Decompress GZiped Kernel\n");
      return EFI_COMPROMISED_DATA;
    }
  } else {
    CopyMem (Kernel, Boot + KernelOffset, KernelSize);
  }

  if (HeaderVersion >= 3) {
    UINTN VendorRamdiskOffset = ALIGN_VALUE (VendorBootHeader->HeaderSize, PageSize);

    CopyMem (Ramdisk, VendorBoot + VendorRamdiskOffset, VendorBootHeader->VendorRamdiskSize);
    CopyMem (Ramdisk + VendorBootHeader->VendorRamdiskSize, Boot + RamdiskOffset, RamdiskSize);

    if (HeaderVersion == 4) {
      UINTN VendorDtbOffset = VendorRamdiskOffset + ALIGN_VALUE (VendorBootHeader->VendorRamdiskSize, PageSize);
      UINTN VendorBootConfigOffset = VendorDtbOffset + ALIGN_VALUE (VendorBootHeader->DtbSize, PageSize);

      CopyMem (Ramdisk + VendorBootHeader->VendorRamdiskSize + RamdiskSize, VendorBoot + VendorBootConfigOffset, VendorBootHeader->BootConfigSize);
    }
  } else {
    CopyMem (Ramdisk, Boot + RamdiskOffset, RamdiskSize);
  }

  return BootLinux (Kernel, AlignedKernelSize, Ramdisk, AlignedRamdiskSize, Fdt);
}

EFI_STATUS
GetBlockIoByPartName (
  EFI_BLOCK_IO_PROTOCOL **BlockIo,
  CHAR16 *PartName)
{
  EFI_STATUS Status;
  EFI_HANDLE *Handles;
  UINTN HandleCount;

  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiBlockIoProtocolGuid, NULL, &HandleCount, &Handles);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to Locate BlockIo Protocol Handle! Status = %r\n", Status));
    return Status;
  }

  for (UINTN i = 0; i < HandleCount; i++) {
    EFI_BLOCK_IO_PROTOCOL *BlkIo;

    Status = gBS->HandleProtocol (Handles[i], &gEfiBlockIoProtocolGuid, (VOID**)&BlkIo);
    if (EFI_ERROR (Status)) {
      continue;
    }

    EFI_PARTITION_INFO_PROTOCOL *PartInfo;

    Status = gBS->HandleProtocol (Handles[i], &gEfiPartitionInfoProtocolGuid, (VOID**)&PartInfo);
    if (EFI_ERROR (Status)) {
      continue;
    }

    if (StrCmp (PartInfo->Info.Gpt.PartitionName, PartName) == 0) {
      *BlockIo = BlkIo;
      break;
    }
  }

  if (*BlockIo == NULL) {
    Status = EFI_NOT_FOUND;
  }

  FreePool (Handles);
  return Status;
}

VOID
WaitKey (
  UINT16 ScanCode)
{
  while (TRUE) {
    EFI_INPUT_KEY Key;

    gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, NULL);
    gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);

    if (Key.ScanCode == ScanCode) {
      break;
    }
  }
}

EFI_STATUS
PatchSiliciumBootImage (
  IN VOID *ImageHeader)
{
  EFI_STATUS Status;
  UINT32 BootHeaderVersion;
  UINT32 SiliciumHeaderVersion;
  BOOT_IMAGE_HEADER_V0 *BootHeaderV0 = ImageHeader;
  BOOT_IMAGE_HEADER_V3 *BootHeaderV3 = ImageHeader;
  BOOT_IMAGE_HEADER_V0 *SiliciumHeaderV0;
  BOOT_IMAGE_HEADER_V3 *SiliciumHeaderV3;
  UINTN KernelOffset;
  UINT32 BootOsPatchVersion;
  UINT32 SiliciumOsPatchVersion;
  EFI_BLOCK_IO_PROTOCOL *BlockIo;
  CHAR16 BootName[8];
  SLOT Slot;
  VOID *Buffer;
  VOID *KernelBuffer;
  UINTN KernelSize;

  BootHeaderVersion = BootHeaderV0->HeaderVersion;

  if (BootHeaderVersion >= 3) {
    BootOsPatchVersion = BootHeaderV3->OsVersion & 0x7FF;
  } else {
    BootOsPatchVersion = BootHeaderV0->OsVersion & 0x7FF;
  }

  Slot = GetActiveSlot ();

  StrCpyS (BootName, sizeof(BootName), Slot == SlotA ? L"boot_a" : Slot == SlotB ? L"boot_b" : L"boot");
  Status = GetBlockIoByPartName (&BlockIo, BootName);
  if (EFI_ERROR (Status) || BlockIo == NULL) {
    Print (L"Failed to Get %s BlockIo Handle! Status = %r\n", BootName, Status);
    return Status;
  }

  Buffer = AllocateZeroPool (BlockIo->Media->BlockSize);
  if (Buffer == NULL) {
    Print (L"Failed to Allocate Memory for Buffer!\n");
    return EFI_OUT_OF_RESOURCES;
  }

  Status = BlockIo->ReadBlocks (BlockIo, BlockIo->Media->MediaId, 0, BlockIo->Media->BlockSize, Buffer);
  if (EFI_ERROR (Status)) {
    Print (L"Failed to Read Header! Status = %r\n", Status);
    goto Cleanup;
  }

  SiliciumHeaderV0 = Buffer;
  SiliciumHeaderV3 = Buffer;
  SiliciumHeaderVersion = SiliciumHeaderV0->HeaderVersion;
  DEBUG ((EFI_D_ERROR, "Silicium Header Version %d\n", SiliciumHeaderVersion));

  if (SiliciumHeaderVersion >= 3) {
    KernelOffset = ALIGN_VALUE (SiliciumHeaderV3->HeaderSize, 4096);
    SiliciumOsPatchVersion = SiliciumHeaderV3->OsVersion & 0x7FF;
  } else {
    KernelOffset = ALIGN_VALUE (sizeof(BOOT_IMAGE_HEADER_V0), SiliciumHeaderV0->PageSize);
    SiliciumOsPatchVersion = SiliciumHeaderV0->OsVersion & 0x7FF;
  }

  if ((KernelOffset / BlockIo->Media->BlockSize) != 0) {
    KernelBuffer = AllocateZeroPool (BlockIo->Media->BlockSize);
    if (KernelBuffer == NULL) {
      Print (L"Failed to Allocate Memory for Kernel Buffer!\n");
      Status = EFI_OUT_OF_RESOURCES;
      goto Cleanup;
    }

    Status = BlockIo->ReadBlocks (BlockIo, BlockIo->Media->MediaId, KernelOffset / BlockIo->Media->BlockSize, BlockIo->Media->BlockSize, KernelBuffer);
    if (EFI_ERROR (Status)) {
      Print (L"Failed to Read Kernel! Status = %r\n", Status);
      goto Cleanup;
    }

    KernelSize = BlockIo->Media->BlockSize;
  } else {
    KernelBuffer = Buffer + KernelOffset;
    KernelSize = BlockIo->Media->BlockSize - KernelOffset;
  }

  {
    CHAR8 Pattern[] = "SILICIUM_UEFI.fd"; // If kernel is gziped
    CHAR8 Pattern2[] = "_FVH"; // If kernel is raw
    BOOLEAN IsSilicium = FALSE;

    for (UINTN i = 0; i < KernelSize; i++) {
      if (CompareMem (KernelBuffer + i, Pattern, sizeof(Pattern) - 1) == 0 || CompareMem(KernelBuffer + i, Pattern2, sizeof(Pattern2) - 1) == 0) {
        IsSilicium = TRUE;
        break;
      }
    }

    if (!IsSilicium) {
      Print (L"Silicium was not found in %s\n", BootName);
      Print (L"AndroidBoot does not support booting via 'fastboot boot'\n");
      Status = EFI_NOT_FOUND;
      goto Cleanup;
    }
  }

  if (BootOsPatchVersion == SiliciumOsPatchVersion) {
    DEBUG ((EFI_D_ERROR, "Silicium OS patch version matches Boot OS patch version\n"));
    goto Cleanup;
  }

  Print (L"Silicium OS patch version does not match Boot OS patch version\n");
  Print (L"Silicium OS patch will be patched in %s\n", BootName);
  Print (L"Silicium will reboot after patching\n");
  Print (L"Press Volume Down to continue\n");

  WaitKey(SCAN_DOWN);

  if (SiliciumHeaderVersion >= 3) {
    SiliciumHeaderV3->OsVersion &= ~0x7FF;
    SiliciumHeaderV3->OsVersion |= BootOsPatchVersion;
  } else {
    SiliciumHeaderV0->OsVersion &= ~0x7FF;
    SiliciumHeaderV0->OsVersion |= BootOsPatchVersion;
  }

  Status = BlockIo->WriteBlocks (BlockIo, BlockIo->Media->MediaId, 0, BlockIo->Media->BlockSize, Buffer);
  if (EFI_ERROR (Status)) {
    Print (L"Failed to Write Header! Status = %r\n", Status);
    goto Cleanup;
  }

  gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);

Cleanup:
  if (KernelSize == BlockIo->Media->BlockSize) {
    FreePool (KernelBuffer);
  }
  FreePool (Buffer);

  return Status;
}

EFI_STATUS
EFIAPI
InitAndroidBoot (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status;
  EFI_FILE_PROTOCOL *Root = NULL;
  EFI_FILE_PROTOCOL *BootImg = NULL;
  EFI_FILE_PROTOCOL *VendorBootImg = NULL;
  EFI_FILE_PROTOCOL *Fdt = NULL;
  VOID *BootImageBuffer = NULL;
  UINTN BootImageSize = 0;
  VOID *VendorBootImageBuffer = NULL;
  UINTN VendorBootImageSize = 0;
  VOID *FdtBuffer = NULL;
  UINTN FdtSize = 0;
  BOOT_IMAGE_HEADER_V0 *ImageHeader;

  EfiBootManagerConnectAll ();

  Status = GetRootFs (&Root);
  if (EFI_ERROR (Status) || Root == NULL) {
    Print (L"Failed to find partition with boot.img\n");
    return EFI_NOT_FOUND;
  }

  Status = Root->Open (Root, &BootImg, L"boot.img", EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR (Status)) {
    Print (L"Failed to open boot.img file\n");
    return Status;
  }

  Status = Root->Open (Root, &Fdt, L"fdt.dtb", EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR (Status)) {
    Print (L"Failed to open fdt.dtb file\n");
    return Status;
  }

  Status = ReadFile (BootImg, &BootImageBuffer, &BootImageSize);
  if (EFI_ERROR (Status) || BootImageBuffer == NULL || BootImageSize == 0) {
    Print (L"Failed to read boot.img file\n");
    return Status;
  }

  Status = ReadFile (Fdt, &FdtBuffer, &FdtSize);
  if (EFI_ERROR (Status) || FdtBuffer == NULL || FdtSize == 0) {
    Print (L"Failed to read fdt.dtb file\n");
    return Status;
  }

  ImageHeader = BootImageBuffer;

  Status = PatchSiliciumBootImage (ImageHeader);
  if (EFI_ERROR(Status)) {
    Print (L"Failed to patch silicium boot image\n");
    return Status;
  }

  if (AsciiStrnCmp (ImageHeader->Magic, BOOT_MAGIC, sizeof(ImageHeader->Magic)) != 0) {
    Print (L"Invalid Boot Image Magic\n");
    return EFI_INVALID_PARAMETER;
  }

  if (ImageHeader->HeaderVersion > 4) {
    Print (L"Invalid boot image version %d\n", ImageHeader->HeaderVersion);
    return EFI_INVALID_PARAMETER;
  }

  if (ImageHeader->HeaderVersion >= 3) {
    Status = Root->Open (Root, &VendorBootImg, L"vendor_boot.img", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR (Status)) {
      Print (L"Failed to open vendor_boot.img file\n");
      return Status;
    }

    Status = ReadFile (VendorBootImg, &VendorBootImageBuffer, &VendorBootImageSize);
    if (EFI_ERROR (Status) || VendorBootImageBuffer == NULL || VendorBootImageSize == 0) {
      Print (L"Failed to read vendor_boot.img file\n");
      return Status;
    }

    VENDOR_BOOT_IMAGE_HEADER_V3 *VendorImageHeader = VendorBootImageBuffer;
    if (AsciiStrnCmp (VendorImageHeader->Magic, VENDOR_BOOT_MAGIC, sizeof(VendorImageHeader->Magic)) != 0) {
      Print (L"Invalid Vendor Boot Image Magic\n");
      return EFI_INVALID_PARAMETER;
    }
  }

  return LoadBoot(BootImageBuffer, VendorBootImageBuffer, FdtBuffer);
}
