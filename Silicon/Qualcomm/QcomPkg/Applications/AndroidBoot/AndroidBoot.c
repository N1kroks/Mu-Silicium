#include <Library/UefiBootManagerLib.h>

#include "AndroidBoot.h"

VOID
WaitKeyDown (
  VOID)
{
  while (TRUE) {
    EFI_INPUT_KEY Key;

    gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, NULL);
    gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);

    if (Key.ScanCode == SCAN_DOWN) {
      break;
    }
  }
}

EFI_STATUS
EFIAPI
InitAndroidBoot (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status;
  VOID *BootImageBuffer = NULL;
  UINTN BootImageSize = 0;
  VOID *VendorBootImageBuffer = NULL;
  UINTN VendorBootImageSize = 0;
  VOID *AndroidImage = NULL;
  UINTN AndroidImageSize = 0;
  UINT32 HeaderVersion = 0;
  VOID *Fdt = NULL;

  EfiBootManagerConnectAll ();

  Status = ReadPartition (L"boot", &BootImageBuffer, &BootImageSize);
  if (EFI_ERROR (Status)) {
    Print (L"Failed to read boot partition! Status = %r\n", Status);
    goto Error;
  }

  Status = FindAndroidBootImage (BootImageBuffer, BootImageSize, &AndroidImage, &AndroidImageSize, &Fdt);
  if (Status == EFI_NOT_FOUND) {
    Print (L"Please append Android boot image and FDT to the end of Silicium boot image\n");
    goto Error;
  } else if (EFI_ERROR (Status)) {
    Print (L"Failed to find Android Boot Image! Status = %r\n", Status);
    goto Error;
  }

  Status = ValidateBootImage (AndroidImage, AndroidImageSize, &HeaderVersion);
  if (EFI_ERROR (Status)) {
    Print (L"Invalid boot image! Status = %r\n", Status);
    goto Error;
  }

  if (HeaderVersion >= 3) {
    Status = ReadPartition (L"vendor_boot", &VendorBootImageBuffer, &VendorBootImageSize);
    if (EFI_ERROR (Status)) {
      Print (L"Failed to read vendor_boot partition! Status = %r\n", Status);
      goto Error;
    }

    Status = ValidateVendorBootImage (VendorBootImageBuffer, VendorBootImageSize, NULL);
    if (EFI_ERROR(Status)) {
      Print (L"Invalid vendor boot image! Status = %r\n", Status);
      goto Error;
    }
  }

  Status = PatchSiliciumBootImage (BootImageBuffer, BootImageSize, AndroidImage, AndroidImageSize);
  if (EFI_ERROR(Status)) {
    Print (L"Failed to patch OS version in Silicium boot image! Status = %r\n", Status);
    goto Error;
  }

  Status = LoadBoot (AndroidImage, AndroidImageSize, VendorBootImageBuffer, VendorBootImageSize, Fdt);
  if (EFI_ERROR(Status)) {
    Print (L"Failed to load boot image! Status = %r\n", Status);
    goto Error;
  }

  return EFI_SUCCESS;
Error:
  Print (L"Press Volume Down to continue\n");
  WaitKeyDown ();

  return Status;
}
