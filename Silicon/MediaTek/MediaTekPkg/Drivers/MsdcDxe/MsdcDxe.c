#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryMapHelperLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>

#include "MsdcDxe.h"

typedef struct {
  VENDOR_DEVICE_PATH Mmc;
  EFI_DEVICE_PATH    End;
} MSDC_DEVICE_PATH;

STATIC MSDC_DEVICE_PATH gDevicePathTemplate = {
  {
    {
      HARDWARE_DEVICE_PATH,
      HW_VENDOR_DP,
      { sizeof (VENDOR_DEVICE_PATH), 0 }
    },
    { 0xb615f1f5, 0x5088, 0x43cd, { 0x80, 0x9c, 0xa1, 0x6e, 0x52, 0x48, 0x7d, 0x00 } }
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    { sizeof (EFI_DEVICE_PATH_PROTOCOL), 0 }
  }
};

STATIC EMMC_DEVICE_PATH gEmmcPathTemplate = {
  {
    MESSAGING_DEVICE_PATH,
    MSG_EMMC_DP,
    { sizeof (EMMC_DEVICE_PATH), 0 }
  },
  0
};

STATIC SD_DEVICE_PATH gSdPathTemplate = {
  {
    MESSAGING_DEVICE_PATH,
    MSG_SD_DP,
    { sizeof (SD_DEVICE_PATH), 0 }
  },
  0
};

EFI_STATUS
MsdcPassThru (
  IN EFI_SD_MMC_PASS_THRU_PROTOCOL           *This,
  IN UINT8                                    Slot,
  IN OUT EFI_SD_MMC_PASS_THRU_COMMAND_PACKET *Packet,
  IN EFI_EVENT                                Event)
{
  if (This == NULL || Packet == NULL || Packet->SdMmcCmdBlk == NULL || Packet->SdMmcStatusBlk == NULL || 
      (Packet->OutDataBuffer == NULL && Packet->OutTransferLength != 0) ||
      (Packet->InDataBuffer == NULL && Packet->InTransferLength != 0)) {
    return EFI_INVALID_PARAMETER;
  }

  return MsdcHwSendCmd (MSDC_CONTEXT_FROM_THIS (This), Packet);
}

EFI_STATUS
MsdcGetNextSlot (
  IN EFI_SD_MMC_PASS_THRU_PROTOCOL *This,
  IN OUT UINT8                     *Slot
  )
{
  if (This == NULL || Slot == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (*Slot == 0xFF) {
    *Slot = 0;
    return EFI_SUCCESS;
  }

  return EFI_NOT_FOUND;
}

EFI_STATUS
MsdcBuildDevicePath (
  IN EFI_SD_MMC_PASS_THRU_PROTOCOL    *This,
  IN UINT8                             Slot,
  OUT EFI_DEVICE_PATH_PROTOCOL       **DevicePath
  )
{
  MSDC_CONTEXT *Ctx;

  if (This == NULL || DevicePath == NULL || Slot != 0) {
    return EFI_INVALID_PARAMETER;
  }

  Ctx = MSDC_CONTEXT_FROM_THIS (This);

  if (Ctx->CardType == CardEmmc) {
    EMMC_DEVICE_PATH *Node = AllocateCopyPool (sizeof (EMMC_DEVICE_PATH), &gEmmcPathTemplate);
    if (Node == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
    *DevicePath = (EFI_DEVICE_PATH_PROTOCOL *)Node;
  } else if (Ctx->CardType == CardSd) {
    SD_DEVICE_PATH *Node = AllocateCopyPool (sizeof (SD_DEVICE_PATH), &gSdPathTemplate);
    if (Node == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
    *DevicePath = (EFI_DEVICE_PATH_PROTOCOL *)Node;
  } else {
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
MsdcGetSlotNumber (
  IN EFI_SD_MMC_PASS_THRU_PROTOCOL    *This,
  IN EFI_DEVICE_PATH_PROTOCOL         *DevicePath,
  OUT UINT8                           *Slot
  )
{
  if (This == NULL || DevicePath == NULL || Slot == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (DevicePath->Type != MESSAGING_DEVICE_PATH || ((DevicePath->SubType != MSG_SD_DP) && (DevicePath->SubType != MSG_EMMC_DP))) {
    return EFI_UNSUPPORTED;
  }

  *Slot = 0;
  return EFI_SUCCESS;
}

EFI_STATUS
MsdcResetDevice (
  IN EFI_SD_MMC_PASS_THRU_PROTOCOL *This,
  IN UINT8                          Slot
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
InitMsdc (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status;
  EFI_MEMORY_REGION_DESCRIPTOR Region;
  CHAR8 Name[32];

  for (UINT8 i = FixedPcdGetBool(PcdStorageIsEMMC) ? 0 : 1; i < gPlatformInfo.NumberOfHosts; i++) {
    MSDC_CONTEXT *Ctx = AllocateZeroPool (sizeof (MSDC_CONTEXT));
    if (Ctx == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    MSDC_DEVICE_PATH *DevicePath = AllocateCopyPool (sizeof (MSDC_DEVICE_PATH), &gDevicePathTemplate);
    if (DevicePath == NULL) {
      FreePool (Ctx);
      return EFI_OUT_OF_RESOURCES;
    }
    DevicePath->Mmc.Guid.Data4[7] = i;

    Ctx->Signature = MSDC_CONTEXT_SIGNATURE;
    Ctx->Index = i;

    Ctx->PassThru.IoAlign = sizeof (UINT32);
    Ctx->PassThru.PassThru = MsdcPassThru;
    Ctx->PassThru.GetNextSlot = MsdcGetNextSlot;
    Ctx->PassThru.BuildDevicePath = MsdcBuildDevicePath;
    Ctx->PassThru.GetSlotNumber = MsdcGetSlotNumber;
    Ctx->PassThru.ResetDevice = MsdcResetDevice;

    AsciiSPrint (Name, sizeof (Name), "MSDC %u", i);
    Status = LocateMemoryRegionByName (Name, &Region);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to Locate %a Memory Region! Status = %r\n", Name, Status));
      FreePool (DevicePath);
      FreePool (Ctx);
      continue;
    }
    Ctx->MmioBase = Region.Address;

    AsciiSPrint (Name, sizeof (Name), "MSDC Top %u", i);
    Status = LocateMemoryRegionByName (Name, &Region);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to Locate %a Memory Region! Status = %r\n", Name, Status));
      FreePool (DevicePath);
      FreePool (Ctx);
      continue;
    }
    Ctx->TopMmioBase = Region.Address;

    Status = MsdcHwInit (Ctx);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to Init MSDC with Index %u! Status = %r\n", i, Status));
      FreePool (DevicePath);
      FreePool (Ctx);
      continue;
    }

    MsdcHwSetBusWidth (Ctx, BusWidth1);

    for (MSDC_CARD_DETECT *Detect = gCardDetectTable; *Detect != NULL; Detect++) {
      Status = (*Detect) (Ctx);
      if (!EFI_ERROR (Status)) {
        break;
      }
    }

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "No Card is Detected at MSDC with Index %u! Status = %r\n", i, Status));
      FreePool (DevicePath);
      FreePool (Ctx);
      continue;
    }

    Status = gBS->InstallMultipleProtocolInterfaces (&Ctx->Handle,
                    &gEfiDevicePathProtocolGuid, DevicePath,
                    &gEfiSdMmcPassThruProtocolGuid, &Ctx->PassThru,
                    NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to Install Protocol for MSDC with Index %u! Status = %r\n", i, Status));
      FreePool (DevicePath);
      FreePool (Ctx);
      continue;
    }
  }

  return EFI_SUCCESS;
}
