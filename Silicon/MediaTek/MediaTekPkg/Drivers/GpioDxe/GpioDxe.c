#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryMapHelperLib.h>
#include <Library/IoLib.h>

#include <Library/GpioImplLib.h>

#include <Protocol/MtkGpio.h>

#define PIN_OFFSET(pin) (((pin) / 32) * 0x10)

#define PIN_MODE_OFFSET(pin) (((pin) / 8) * 0x10)
#define PIN_MODE_BIT(pin) (((pin) % 8) * 4)

STATIC
UINT32
GpioRead(
  IN  UINT32 Reg,
  IN  UINTN  Index)
{
  if (Index >= gPinctrlBankRegionCount) {
    DEBUG ((DEBUG_ERROR, "Invalid Pinctrl Bank Index: %u\n", Index));
    ASSERT (FALSE);
  }

  return MmioRead32 (gPinctrlBankRegions[Index] + Reg);
}

STATIC
VOID
GpioWrite(
  IN  UINT32 Reg,
  IN  UINTN  Index,
  IN  UINT32 Value)
{
  if (Index >= gPinctrlBankRegionCount) {
    DEBUG ((DEBUG_ERROR, "Invalid Pinctrl Bank Index: %u\n", Index));
    ASSERT (FALSE);
  }

  MmioWrite32 (gPinctrlBankRegions[Index] + Reg, Value);
}

EFI_STATUS
GpioGetDir(
  IN  UINT32   Pin,
  OUT MTK_GPIO_DIR *Direction)
{
  UINT32 Value;

  if (Pin > gPlatformInfo.MaxPin)
    return EFI_INVALID_PARAMETER;

  // Read Pin Direction
  Value = GpioRead (gPlatformInfo.DirOffset + PIN_OFFSET(Pin), 0);
  *Direction = (Value & (1 << (Pin % 32))) ? DirOut : DirIn;

  return EFI_SUCCESS;
}

EFI_STATUS
GpioSetDir(
  IN UINT32  Pin,
  IN MTK_GPIO_DIR Direction)
{
  UINT32 Value, Offset;

  if (Pin > gPlatformInfo.MaxPin)
    return EFI_INVALID_PARAMETER;

  // Set Pin Direction
  Offset = PIN_OFFSET (Pin) + (Direction == DirIn ? gPlatformInfo.ResetOffset : gPlatformInfo.SetOffset);
  Value = GpioRead (Offset, 0);
  Value |= (1 << (Pin % 32));
  GpioWrite (Offset, 0, Value);

  return EFI_SUCCESS;
}

EFI_STATUS
GpioGetState(
  IN  UINT32   Pin,
  OUT BOOLEAN *State)
{
  UINT32 Value;
  MTK_GPIO_DIR Direction;

  if (Pin > gPlatformInfo.MaxPin)
    return EFI_INVALID_PARAMETER;

  // Read Pin State
  GpioGetDir (Pin, &Direction);
  Value = GpioRead ((Direction == DirIn ? gPlatformInfo.DataInOffset : gPlatformInfo.DataOutOffset) + PIN_OFFSET(Pin), 0);
  *State = !!(Value & (1 << (Pin % 32)));

  return EFI_SUCCESS;
}

EFI_STATUS
GpioSetState(
  IN UINT32  Pin,
  IN BOOLEAN State)
{
  UINT32 Value, Offset;

  if (Pin > gPlatformInfo.MaxPin)
    return EFI_INVALID_PARAMETER;

  // Set Pin State
  Offset = gPlatformInfo.DataOutOffset + (State ? gPlatformInfo.SetOffset : gPlatformInfo.ResetOffset) + PIN_OFFSET(Pin);
  Value = GpioRead (Offset, 0);
  Value |= (1 << (Pin % 32));
  GpioWrite (Offset, 0, Value);

  return EFI_SUCCESS;
}

EFI_STATUS
GpioSetMode(
  IN UINT32 Pin,
  IN UINT32 Mode)
{
  UINT32 SetValue, ResetValue, Offset;
  UINT32 ModeBits = (Mode << PIN_MODE_BIT (Pin));

  if (Pin > gPlatformInfo.MaxPin)
    return EFI_INVALID_PARAMETER;

  Offset = gPlatformInfo.ModeOffset + PIN_MODE_OFFSET (Pin);

  // Read current Mode
  SetValue = GpioRead (Offset + gPlatformInfo.SetOffset, 0);
  ResetValue = GpioRead (Offset + gPlatformInfo.ResetOffset, 0);

  // Set new Mode
  SetValue |= ModeBits;
  ResetValue |= (~ModeBits) & (0x7 << PIN_MODE_BIT (Pin));

  // Write new Mode
  GpioWrite (Offset + gPlatformInfo.SetOffset, 0, SetValue);
  GpioWrite (Offset + gPlatformInfo.ResetOffset, 0, ResetValue);

  return EFI_SUCCESS;
}

EFI_STATUS
GpioSetDriveStrength(
  IN UINT32 Pin,
  IN UINT32 DriveStrength)
{
  UINT32 Value;
  MTK_GPIO_DRIVE_STRENGTH_TABLE Table = gDriveStrengthTable[Pin];

  if (Pin > gPlatformInfo.MaxPin || DriveStrength > Table.Mask)
    return EFI_INVALID_PARAMETER;

  DriveStrength &= Table.Mask;

  Value = GpioRead (Table.Offset, Table.Region);
  Value &= ~(Table.Mask << Table.Shift);
  Value |= (DriveStrength << Table.Shift);
  GpioWrite (Table.Offset, Table.Region, Value);

  return EFI_SUCCESS;
}

EFI_STATUS
GpioSetBias(
  IN UINT32              Pin,
  IN MTK_GPIO_BIAS       Bias,
  IN MTK_GPIO_RESISTANCE Resistance)
{
  UINT32 Value;
  BOOLEAN R0, R1;
  MTK_GPIO_BIAS_TABLE *BiasTable = NULL;
  MTK_GPIO_RESISTANCE_TABLE *ResistanceTable = NULL;

  if (Pin > gPlatformInfo.MaxPin)
    return EFI_INVALID_PARAMETER;

  for (UINTN i = 0; i < gBiasTableCount; i++) {
    if (gBiasTable[i].Pin == Pin) {
      BiasTable = &gBiasTable[i];
      break;
    }
  }

  for (UINTN i = 0; i < gResistanceTableCount; i++) {
    if (gResistanceTable[i].Pin == Pin) {
      ResistanceTable = &gResistanceTable[i];
      break;
    }
  }

  if (!BiasTable || !ResistanceTable) {
    return EFI_UNSUPPORTED;
  }

  switch (Resistance) {
  case ResistanceR1R0_00:
    Bias = BiasDisabled;
    R0 = FALSE;
    R1 = FALSE;
    break;
  case ResistanceR1R0_01:
    R0 = TRUE;
    R1 = FALSE;
    break;
  case ResistanceR1R0_10:
    R0 = FALSE;
    R1 = TRUE;
    break;
  case ResistanceR1R0_11:
    R0 = TRUE;
    R1 = TRUE;
    break;
  default:
    return EFI_INVALID_PARAMETER;
  }

  Value = GpioRead (BiasTable->Offset, BiasTable->Region);
  Value &= ~(BiasTable->Mask << BiasTable->Shift);
  Value |= (Bias << BiasTable->Shift);
  GpioWrite (BiasTable->Offset, BiasTable->Region, Value);

  Value = GpioRead (ResistanceTable->OffsetR0, ResistanceTable->Region);
  Value &= ~(ResistanceTable->Mask << ResistanceTable->Shift);
  Value |= (R0 << ResistanceTable->Shift);
  GpioWrite (ResistanceTable->OffsetR0, ResistanceTable->Region, Value);

  Value = GpioRead (ResistanceTable->OffsetR1, ResistanceTable->Region);
  Value &= ~(ResistanceTable->Mask << ResistanceTable->Shift);
  Value |= (R1 << ResistanceTable->Shift);
  GpioWrite (ResistanceTable->OffsetR1, ResistanceTable->Region, Value);

  return EFI_SUCCESS;
}

STATIC MTK_GPIO_PROTOCOL mGpio = {
  GpioGetDir,
  GpioSetDir,
  GpioGetState,
  GpioSetState,
  GpioSetMode,
  GpioSetDriveStrength,
  GpioSetBias,
};

EFI_STATUS
EFIAPI
InitGpioDriver (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status;

  // Register GPIO Protocol
  Status = gBS->InstallMultipleProtocolInterfaces (&ImageHandle, &gMediaTekGpioProtocolGuid, &mGpio, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to Register Gpio Protocol! Status = %r\n", Status));
    return Status;
  }

  return Status;
}