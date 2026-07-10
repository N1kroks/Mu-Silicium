#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/MsdcImplLib.h>

#include <Protocol/MtkGpio.h>
#include <Protocol/MtkClock.h>
#include <Protocol/MtkPmic.h>

MSDC_PLATFORM_INFO gPlatformInfo = {
  .NumberOfHosts = 2,
  .UseTop = TRUE,
  .MsdcPadTuneReg = 0xf0,
  .TuningStep = {32, 64},
  .AsyncFifo = TRUE,
  .BusyCheck = TRUE,
  .StopClkFix = TRUE,
  .StopDlySel = 3,
  .EnhanceRx = TRUE,
  .Support64g = TRUE,
  .DataTune = TRUE
};

STATIC MTK_GPIO_PROTOCOL *mGpio = NULL;
STATIC MTK_CLOCK_PROTOCOL *mClock = NULL;
STATIC MTK_PMIC_PROTOCOL *mPmic = NULL;

EFI_STATUS
GetSourceClockRate (
  UINT32 Index,
  UINTN *Hz)
{
  EFI_STATUS Status;
  UINT32 ClockId;

  // Get Clock Id
  Status = mClock->GetId(Index == 0 ? "TOP_MSDC50_0" : "TOP_MSDC30_1", &ClockId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Get %a Clock Id! Status = %r\n", Index == 0 ? "TOP_MSDC50_0" : "TOP_MSDC30_1", Status));
    return Status;
  }

  // Get Clock Frequency
  Status = mClock->GetFrequency(ClockId, Hz);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Get %a Clock Frequency! Status = %r\n", Index == 0 ? "TOP_MSDC50_0" : "TOP_MSDC30_1", Status));
    return Status;
  }

  return Status;
}

EFI_STATUS
SourceClockControl (
  UINT32 Index,
  BOOLEAN Enable)
{
  EFI_STATUS Status;
  UINT32 ClockId;

  if (Index == 0) {
    // Get Clock Id
    Status = mClock->GetId("INFRA_FAES_FDE", &ClockId);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to Get INFRA_FAES_FDE Clock Id! Status = %r\n", Status));
      return Status;
    }

    // Enable Clock
    Status = mClock->SetEnable(ClockId, Enable);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to Enable INFRA_FAES_FDE Clock! Status = %r\n", Status));
      return Status;
    }
  }

  // Get Clock Id
  Status = mClock->GetId(Index == 0 ? "INFRA_MSDC0_SRC" : "INFRA_MSDC1_SRC", &ClockId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Get INFRA_MSDC%d_SRC Clock Id! Status = %r\n", Index, Status));
    return Status;
  }

  // Enable Clock
  Status = mClock->SetEnable(ClockId, Enable);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Enable INFRA_MSDC%d_SRC Clock! Status = %r\n", Index, Status));
    return Status;
  }

  return Status;
}

EFI_STATUS
ClockControl (
  UINT32 Index,
  BOOLEAN Enable)
{
  EFI_STATUS Status;
  UINT32 ClockId;

  // Get Clock Id
  Status = mClock->GetId(Index == 0 ? "TOP_MSDC50_0" : "TOP_MSDC30_1", &ClockId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Get %a Clock Id! Status = %r\n", Index == 0 ? "TOP_MSDC50_0" : "TOP_MSDC30_1", Status));
    return Status;
  }

  // Enable Clock
  Status = mClock->SetEnable(ClockId, Enable);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Enable %a Clock! Status = %r\n", Index == 0 ? "TOP_MSDC50_0" : "TOP_MSDC30_1", Status));
    return Status;
  }

  // Get Clock Id
  Status = mClock->GetId(Index == 0 ? "INFRA_MSDC0" : "INFRA_MSDC1", &ClockId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Get INFRA_MSDC%d Clock Id! Status = %r\n", Index, Status));
    return Status;
  }

  // Enable Clock
  Status = mClock->SetEnable(ClockId, Enable);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Enable INFRA_MSDC%d Clock! Status = %r\n", Index, Status));
    return Status;
  }

  return Status;
}

EFI_STATUS
PowerControl (
  UINT32 Index,
  BOOLEAN Enable,
  UINT32 VoltageLevel)
{
  EFI_STATUS Status;

  if (Index == 0) {
    // Enable VEMC LDO
    Status = mPmic->RegulatorSetEnable("ldo_vemc", Enable);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to Enable LDO VEMC! Status = %r\n", Status));
      return Status;
    }
  } else if (Index == 1) {
    // Set VMC LDO Voltage
    Status = mPmic->RegulatorSetVoltage("ldo_vmc", VoltageLevel);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to Set VMC LDO Voltage! Status = %r\n", Status));
      return Status;
    }

    // Enable VMCH LDO
    Status = mPmic->RegulatorSetEnable("ldo_vmch", Enable);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to Enable LDO VMCH! Status = %r\n", Status));
      return Status;
    }

    // Enable VMC LDO
    Status = mPmic->RegulatorSetEnable("ldo_vmc", Enable);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to Enable LDO VMC! Status = %r\n", Status));
      return Status;
    }
  }

  return Status;
}

VOID
ConfigureGpio (
  UINT32 Index)
{
  if (Index == 0) {
    // Setup EMMC GPIO
    // DAT0-DAT7: GPIO123, GPIO128, GPIO125, GPIO132, GPIO126, GPIO129, GPIO127, GPIO130
    // DS: GPIO131
    // CMD: GPIO122
    // CLK: GPIO124
    for (UINTN i = 122; i <= 132; i++) {
      mGpio->SetMode (i, 1);
      mGpio->SetDriveStrength (i, 4);
      if (i == 124 || i == 131) {
        mGpio->SetBias (i, BiasPullDown, ResistanceR1R0_10);
        if (i == 124) {
          mGpio->SetDir (i, DirOut);
        } else {
          mGpio->SetDir (i, DirIn);
        }
      } else {
        mGpio->SetBias (i, BiasPullUp, ResistanceR1R0_01);
        mGpio->SetDir (i, DirIn);
      }
    }
  } else {
    // Setup SDCard GPIO
    // DAT0-DAT3: GPIO161-164
    // CMD: GPIO170
    // CLK: GPIO171
    for (UINTN i = 161; i <= 171; i++) {
      if (i >= 165 && i <= 169) {
        continue;
      }

      mGpio->SetMode (i, 1);
      if (i == 171) {
        mGpio->SetBias (i, BiasPullDown, ResistanceR1R0_10);
        mGpio->SetDir (i, DirOut);
      } else {
        mGpio->SetBias (i, BiasPullUp, ResistanceR1R0_01);
        mGpio->SetDir (i, DirIn);
        mGpio->SetDriveStrength (i, 4);
      }
    }
  }
}

EFI_STATUS
EFIAPI
MsdcLibConstructor (VOID)
{
  EFI_STATUS Status;

  // Locate Gpio Protocol
  Status = gBS->LocateProtocol (&gMediaTekGpioProtocolGuid, NULL, (VOID **)&mGpio);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Locate GPIO Protocol! Status = %r\n", Status));
    ASSERT_EFI_ERROR (Status);
  }

  // Locate Clock Protocol
  Status = gBS->LocateProtocol (&gMediaTekClockProtocolGuid, NULL, (VOID **)&mClock);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Locate Clock Protocol! Status = %r\n", Status));
    ASSERT_EFI_ERROR (Status);
  }

  // Locate PMIC Protocol
  Status = gBS->LocateProtocol (&gMediaTekPmicProtocolGuid, NULL, (VOID **)&mPmic);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Locate PMIC Protocol! Status = %r\n", Status));
    ASSERT_EFI_ERROR (Status);
  }

  // Set VMCH LDO Voltage
  Status = mPmic->RegulatorSetVoltage("ldo_vmch", 3300000);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Set VMCH LDO Voltage! Status = %r\n", Status));
    ASSERT_EFI_ERROR (Status);
  }

  return Status;
}