#ifndef _ANDROID_BOOT_H_
#define _ANDROID_BOOT_H_

#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

typedef VOID (*LINUX_KERNEL) (
  UINT64 X0,
  UINT64 X1,
  UINT64 X2,
  UINT64 X3
);

VOID
WaitKeyDown (
  VOID
);

EFI_STATUS
ReadPartition (
  IN CONST CHAR16 *PartName,
  OUT VOID       **Buffer,
  OUT UINTN       *BufferSize
  );

EFI_STATUS
WritePartition (
  IN CONST CHAR16 *PartName,
  IN VOID         *Buffer,
  IN UINTN         BufferSize
  );

EFI_STATUS
ValidateBootImage (
  IN  CONST VOID *AndroidImage,
  IN  UINTN       AndroidImageSize,
  OUT UINT32     *HeaderVersion
  );

EFI_STATUS
ValidateVendorBootImage (
  IN  CONST VOID *VendorImage,
  IN  UINTN       VendorImageSize,
  OUT UINT32     *VendorHeaderVersion
  );

EFI_STATUS
FindAndroidBootImage (
  IN VOID    *Buffer,
  IN UINTN    BufferSize,
  OUT VOID  **AndroidImage,
  OUT UINTN  *AndroidImageSize,
  OUT VOID  **Fdt
  );

EFI_STATUS
PatchSiliciumBootImage (
  IN VOID *Buffer,
  IN UINTN BufferSize,
  IN VOID *AndroidImage,
  IN UINTN AndroidImageSize
  );

EFI_STATUS
LoadBoot (
  IN VOID  *Boot,
  IN UINTN  BootSize,
  IN VOID  *VendorBoot,
  IN UINTN  VendorBootSize,
  IN VOID  *Fdt
  );

EFI_STATUS
BootLinux (
  IN VOID  *Kernel,
  IN UINTN  KernelSize,
  IN VOID  *Ramdisk,
  IN UINTN  RamdiskSize,
  IN VOID  *Fdt
  );

#endif