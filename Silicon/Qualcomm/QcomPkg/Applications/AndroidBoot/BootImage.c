#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryMapHelperLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/FdtLib.h>

#include "tinf/tinf.h"
#include "AndroidBoot.h"

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
  CHAR8  Name[16];
  CHAR8  Cmdline[512];
  UINT32 Id[8];
  CHAR8  ExtraCmdline[1024];
} BOOT_IMAGE_HEADER_V0;

typedef struct {
  CHAR8  Magic[8];
  UINT32 KernelSize;
  UINT32 RamdiskSize;
  UINT32 OsVersion;
  UINT32 HeaderSize;
  UINT32 Reserved[4];
  UINT32 HeaderVersion;
  CHAR8  Cmdline[1536];
} BOOT_IMAGE_HEADER_V3;

typedef struct {
  CHAR8  Magic[8];
  UINT32 HeaderVersion;
  UINT32 PageSize;
  UINT32 KernelAddr;
  UINT32 RamdiskAddr;
  UINT32 VendorRamdiskSize;
  CHAR8  Cmdline[2048];
  UINT32 TagsAddress;
  CHAR8  Name[16];
  UINT32 HeaderSize;
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

EFI_STATUS
ValidateBootImage (
  IN  CONST VOID *AndroidImage,
  IN  UINTN       AndroidImageSize,
  OUT UINT32     *HeaderVersion)
{
  BOOT_IMAGE_HEADER_V0 *Header;

  if (AndroidImage == NULL || AndroidImageSize < sizeof (BOOT_IMAGE_HEADER_V0)) {
    return EFI_INVALID_PARAMETER;
  }

  Header = (BOOT_IMAGE_HEADER_V0 *)AndroidImage;
  if (AsciiStrnCmp (Header->Magic, BOOT_MAGIC, sizeof (Header->Magic)) != 0) {
    return EFI_INVALID_PARAMETER;
  }

  if (HeaderVersion != NULL) {
    *HeaderVersion = Header->HeaderVersion;
  }

  if (Header->HeaderVersion > 4) {
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
ValidateVendorBootImage (
  IN  CONST VOID *VendorImage,
  IN  UINTN       VendorImageSize,
  OUT UINT32     *VendorHeaderVersion)
{
  VENDOR_BOOT_IMAGE_HEADER_V3 *Header;

  if (VendorImage == NULL || VendorImageSize < sizeof (VENDOR_BOOT_IMAGE_HEADER_V3)) {
    return EFI_INVALID_PARAMETER;
  }

  Header = (VENDOR_BOOT_IMAGE_HEADER_V3 *)VendorImage;
  if (AsciiStrnCmp (Header->Magic, VENDOR_BOOT_MAGIC, sizeof (Header->Magic)) != 0) {
    return EFI_INVALID_PARAMETER;
  }

  if (VendorHeaderVersion != NULL) {
    *VendorHeaderVersion = Header->HeaderVersion;
  }

  if (Header->HeaderVersion < 3 || Header->HeaderVersion > 4) {
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
GetBootImageSize(
  IN VOID   *Buffer,
  IN UINTN   BufferSize,
  OUT UINTN *ImageSize)
{
  EFI_STATUS Status;
  UINT32 HeaderVersion;
  UINT32 KernelSize;
  UINT32 RamdiskSize;
  UINT32 HeaderSize;

  if (Buffer == NULL || BufferSize == 0 || ImageSize == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = ValidateBootImage (Buffer, BufferSize, &HeaderVersion);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  if (HeaderVersion >= 3) {
    BOOT_IMAGE_HEADER_V3 *Header = (BOOT_IMAGE_HEADER_V3 *)Buffer;

    // Actually there should be page size from vendor boot header
    KernelSize = ALIGN_VALUE (Header->KernelSize, EFI_PAGE_SIZE);
    RamdiskSize = ALIGN_VALUE (Header->RamdiskSize, EFI_PAGE_SIZE);
    HeaderSize = ALIGN_VALUE (Header->HeaderSize, EFI_PAGE_SIZE);
  } else {
    BOOT_IMAGE_HEADER_V0 *Header = (BOOT_IMAGE_HEADER_V0 *)Buffer;

    KernelSize = ALIGN_VALUE (Header->KernelSize, Header->PageSize);
    RamdiskSize = ALIGN_VALUE (Header->RamdiskSize, Header->PageSize);
    HeaderSize = ALIGN_VALUE (sizeof (BOOT_IMAGE_HEADER_V0), Header->PageSize);
  }

  *ImageSize = KernelSize + RamdiskSize + HeaderSize;

  return EFI_SUCCESS;
}

EFI_STATUS
FindAndroidBootImage (
  IN VOID    *Buffer,
  IN UINTN    BufferSize,
  OUT VOID  **AndroidImage,
  OUT UINTN  *AndroidImageSize,
  OUT VOID  **Fdt)
{
  EFI_STATUS Status;
  VOID *Image;
  VOID *Devicetree;
  UINTN SiliciumSize;
  UINTN ImageSize;

  if (AndroidImage == NULL || AndroidImageSize == NULL || Fdt == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = GetBootImageSize (Buffer, BufferSize, &SiliciumSize);
  if (EFI_ERROR(Status)) {
    return EFI_NOT_FOUND;
  }

  Image = Buffer + SiliciumSize;
  ImageSize = (UINTN)(Buffer + BufferSize - Image);

  Status = GetBootImageSize (Image, ImageSize, &ImageSize);
  if (EFI_ERROR(Status)) {
    return EFI_NOT_FOUND;
  }

  Devicetree = Image + ImageSize;
  if (*(UINT32 *)Devicetree != DTB_MAGIC) {
    return EFI_NOT_FOUND;
  }

  *AndroidImage = Image;
  *AndroidImageSize = ImageSize;
  *Fdt = Devicetree;

  return EFI_SUCCESS;
}

STATIC
BOOLEAN
IsSiliciumKernel (
  IN VOID *Buffer,
  IN UINTN BufferSize)
{
  STATIC CONST CHAR8 SiliciumPattern[] = "SILICIUM_UEFI.fd";
  STATIC CONST CHAR8 FVPattern[] = "_FVH";

  if (Buffer == NULL || BufferSize == 0) {
    return FALSE;
  }

  for (UINTN i = 0; i < BufferSize; i++) {
    if (CompareMem (Buffer + i, SiliciumPattern, sizeof (SiliciumPattern) - 1) == 0) {
      return TRUE;
    }

    if (CompareMem (Buffer + i, FVPattern, sizeof (FVPattern) - 1) == 0) {
      return TRUE;
    }
  }

  return FALSE;
}

EFI_STATUS
PatchSiliciumBootImage (
  IN VOID *Buffer,
  IN UINTN BufferSize,
  IN VOID *AndroidImage,
  IN UINTN AndroidImageSize)
{
  EFI_STATUS Status;
  UINT32     SiliciumHeaderVersion;
  UINT32     SiliciumOsPatchVersion;
  UINT32     AndroidHeaderVersion;
  UINT32     AndroidOsPatchVersion;
  UINT32     HeaderSize;
  UINT32     KernelOffset;

  if (Buffer == NULL || AndroidImage == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = ValidateBootImage (Buffer, BufferSize, &SiliciumHeaderVersion);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (SiliciumHeaderVersion >= 3) {
    BOOT_IMAGE_HEADER_V3 *Header = (BOOT_IMAGE_HEADER_V3 *)Buffer;
    SiliciumOsPatchVersion = Header->OsVersion & 0x7FF;
    KernelOffset = ALIGN_VALUE (Header->HeaderSize, EFI_PAGE_SIZE);
  } else {
    BOOT_IMAGE_HEADER_V0 *Header = (BOOT_IMAGE_HEADER_V0 *)Buffer;
    SiliciumOsPatchVersion = Header->OsVersion & 0x7FF;
    KernelOffset = ALIGN_VALUE (sizeof (BOOT_IMAGE_HEADER_V0), Header->PageSize);
  }

  if (IsSiliciumKernel (Buffer + KernelOffset, 0x100) == FALSE) {
    Print (L"Silicium image was not found in the boot partition\n");
    return EFI_COMPROMISED_DATA;
  }

  Status = ValidateBootImage (AndroidImage, AndroidImageSize, &AndroidHeaderVersion);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (AndroidHeaderVersion >= 3) {
    BOOT_IMAGE_HEADER_V3 *Header = (BOOT_IMAGE_HEADER_V3 *)AndroidImage;
    AndroidOsPatchVersion = Header->OsVersion & 0x7FF;
  } else {
    BOOT_IMAGE_HEADER_V0 *Header = (BOOT_IMAGE_HEADER_V0 *)AndroidImage;
    AndroidOsPatchVersion = Header->OsVersion & 0x7FF;
  }

  if (AndroidOsPatchVersion == SiliciumOsPatchVersion) {
    return EFI_SUCCESS;
  }

  Print (L"Android and Silicium OS patch versions do not match\n");
  Print (L"Silicium OS patch version will be patched in the boot partition\n");
  Print (L"The device will reboot to apply the change\n");
  Print (L"Press Volume Down to continue\n");

  WaitKeyDown ();

  if (SiliciumHeaderVersion >= 3) {
    BOOT_IMAGE_HEADER_V3 *Header = (BOOT_IMAGE_HEADER_V3 *)Buffer;
    Header->OsVersion &= ~0x7FF;
    Header->OsVersion |= AndroidOsPatchVersion;

    HeaderSize = ALIGN_VALUE (Header->HeaderSize, EFI_PAGE_SIZE);
  } else {
    BOOT_IMAGE_HEADER_V0 *Header = (BOOT_IMAGE_HEADER_V0 *)Buffer;
    Header->OsVersion &= ~0x7FF;
    Header->OsVersion |= AndroidOsPatchVersion;

    HeaderSize = ALIGN_VALUE (sizeof (BOOT_IMAGE_HEADER_V0), Header->PageSize);
  }

  Status = WritePartition (L"boot", Buffer, HeaderSize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);

  return EFI_SUCCESS;
}

STATIC
BOOLEAN
IsRangeValid(
  IN UINTN Offset,
  IN UINTN Length,
  IN UINTN ImageSize)
{
  return (Offset + Length) <= ImageSize;
}

STATIC
BOOLEAN
IsGzip (
  IN UINT8 *Buffer,
  IN UINTN  Length)
{
  if (Buffer == NULL || Length < 18) {
    return FALSE;
  }

  return Buffer[0] == 0x1f && Buffer[1] == 0x8b && Buffer[2] == 0x08;
}

EFI_STATUS
LoadBoot (
  IN VOID  *Boot,
  IN UINTN  BootSize,
  IN VOID  *VendorBoot,
  IN UINTN  VendorBootSize,
  IN VOID  *Fdt)
{
  EFI_STATUS Status;
  UINT32 HeaderVersion;
  UINT32 VendorHeaderVersion;
  UINT32 KernelSize;
  UINT32 AlignedKernelSize;
  UINT32 KernelOffset;
  UINT32 RamdiskSize;
  UINT32 AlignedRamdiskSize;
  UINT32 RamdiskOffset;
  UINT32 VendorRamDiskSize;
  UINT32 VendorRamdiskOffset;
  UINT32 VendorDtbSize;
  UINT32 VendorDtbOffset;
  UINT32 VendorBootConfigSize;
  UINT32 VendorBootConfigOffset;
  BOOLEAN GzipKernel;
  EFI_MEMORY_REGION_DESCRIPTOR KernelRegion;
  UINT32 KernelTextOffset = 0x80000;
  UINTN KernelRegionSize;
  VOID *Kernel;
  VOID *Ramdisk = NULL;

  if (Boot == NULL || BootSize == 0 || Fdt == NULL) {
    Print (L"Invalid boot image or FDT\n");
    return EFI_INVALID_PARAMETER;
  }

  Status = ValidateBootImage (Boot, BootSize, &HeaderVersion);
  if (EFI_ERROR (Status)) {
    Print (L"Invalid boot image! Status = %r\n", Status);
    return Status;
  }

  if (HeaderVersion >= 3) {
    BOOT_IMAGE_HEADER_V3 *Header = Boot;
    VENDOR_BOOT_IMAGE_HEADER_V3 *VendorHeader = VendorBoot;

    Status = ValidateVendorBootImage(VendorBoot, VendorBootSize, &VendorHeaderVersion);
    if (EFI_ERROR (Status)) {
      Print (L"Invalid vendor boot image! Status = %r\n", Status);
      return Status;
    }

    KernelSize = Header->KernelSize;
    AlignedKernelSize = ALIGN_VALUE (KernelSize, VendorHeader->PageSize);
    KernelOffset = ALIGN_VALUE (Header->HeaderSize, VendorHeader->PageSize);

    RamdiskSize = Header->RamdiskSize;
    RamdiskOffset = KernelOffset + AlignedKernelSize;

    VendorRamDiskSize = VendorHeader->VendorRamdiskSize;
    VendorRamdiskOffset = ALIGN_VALUE (VendorHeader->HeaderSize, VendorHeader->PageSize);

    AlignedRamdiskSize = ALIGN_VALUE (RamdiskSize + VendorRamDiskSize, VendorHeader->PageSize);

    if (!IsRangeValid (VendorRamdiskOffset, VendorRamDiskSize, BootSize)) {
      Print (L"Vendor ramdisk is out of range\n");
      return EFI_INVALID_PARAMETER;
    }

    VendorDtbSize = VendorHeader->DtbSize;
    VendorDtbOffset = VendorRamdiskOffset + ALIGN_VALUE (VendorRamDiskSize, VendorHeader->PageSize);

    if (!IsRangeValid (VendorDtbOffset, VendorDtbSize, BootSize)) {
      Print (L"Vendor DTB is out of range\n");
      return EFI_INVALID_PARAMETER;
    }

    if (HeaderVersion == 4) {
      AlignedRamdiskSize = ALIGN_VALUE (AlignedRamdiskSize + VendorHeader->BootConfigSize, VendorHeader->PageSize);

      VendorBootConfigSize = VendorHeader->BootConfigSize;
      VendorBootConfigOffset = VendorDtbOffset + ALIGN_VALUE (VendorDtbSize, VendorHeader->PageSize);

      if (!IsRangeValid (VendorBootConfigOffset, VendorBootConfigSize, BootSize)) {
        Print (L"Vendor boot config is out of range\n");
        return EFI_INVALID_PARAMETER;
      }
    }
  } else {
    BOOT_IMAGE_HEADER_V0 *Header = Boot;

    KernelSize = Header->KernelSize;
    AlignedKernelSize = ALIGN_VALUE (KernelSize, Header->PageSize);
    KernelOffset = ALIGN_VALUE (sizeof (BOOT_IMAGE_HEADER_V0), Header->PageSize);

    RamdiskSize = Header->RamdiskSize;
    AlignedRamdiskSize = ALIGN_VALUE (RamdiskSize, Header->PageSize);
    RamdiskOffset = KernelOffset + AlignedKernelSize;
  }

  if (!IsRangeValid (KernelOffset, KernelSize, BootSize)) {
    Print (L"Kernel is out of range\n");
    return EFI_INVALID_PARAMETER;
  }

  if (!IsRangeValid (RamdiskOffset, RamdiskSize, BootSize)) {
    Print (L"Ramdisk is out of range\n");
    return EFI_INVALID_PARAMETER;
  }

  GzipKernel = IsGzip (Boot + KernelOffset, KernelSize);
  if (!GzipKernel) {
    KERNEL_HEADER *KernelHeader = Boot + KernelOffset;

    if (KernelHeader->ImageSize != 0) {
      KernelTextOffset = KernelHeader->TextOffset;
    }
  }

  KernelRegionSize = KernelSize + KernelTextOffset;
  Status = LocateMemoryRegionByName ("Kernel", &KernelRegion);
  if (!EFI_ERROR (Status) && KernelRegion.Length >= KernelRegionSize) {
    Kernel = (VOID *)(UINTN)KernelRegion.Address;
  } else {
    Print (L"Kernel region is too small, allocating kernel memory\n");
    Kernel = AllocateAlignedPages (EFI_SIZE_TO_PAGES (KernelRegionSize), SIZE_2MB);
    if (Kernel == NULL) {
      Print (L"Failed to allocate memory for kernel\n");
      return EFI_OUT_OF_RESOURCES;
    }
  }

  SetMem (Kernel, KernelRegionSize, 0);

  Kernel += KernelTextOffset;

  if (AlignedRamdiskSize != 0) {
    Ramdisk = AllocatePages (EFI_SIZE_TO_PAGES (AlignedRamdiskSize));
    if (Ramdisk == NULL) {
      FreePages (Kernel, EFI_SIZE_TO_PAGES (KernelRegionSize));
      Print (L"Failed to allocate memory for ramdisk\n");
      return EFI_OUT_OF_RESOURCES;
    }
  }

  SetMem (Ramdisk, AlignedRamdiskSize, 0);

  if (GzipKernel) {
    UINT32 DestLen = AlignedKernelSize;
    if (tinf_gzip_uncompress (Kernel, &DestLen, Boot + KernelOffset, KernelSize) != TINF_OK) {
      Print (L"Failed to unpack gzip kernel\n");
      return EFI_COMPROMISED_DATA;
    }
  } else {
    CopyMem (Kernel, Boot + KernelOffset, KernelSize);
  }

  if (HeaderVersion >= 3) {
    if (VendorRamDiskSize != 0) {
      CopyMem (Ramdisk, VendorBoot + VendorRamdiskOffset, VendorRamDiskSize);
    }

    if (RamdiskSize != 0) {
      CopyMem (Ramdisk + VendorRamDiskSize, Boot + RamdiskOffset, RamdiskSize);
    }

    if (HeaderVersion == 4) {
      CopyMem (Ramdisk + VendorRamDiskSize + RamdiskSize, VendorBoot + VendorBootConfigOffset, VendorBootConfigSize);
    }
  } else if (RamdiskSize != 0) {
    CopyMem (Ramdisk, Boot + RamdiskOffset, RamdiskSize);
  }

  return BootLinux (Kernel, AlignedKernelSize, Ramdisk, AlignedRamdiskSize, Fdt);
}