#include <Library/BaseMemoryLib.h>
#include <Library/TimerLib.h>
#include <IndustryStandard/Emmc.h>
#include <IndustryStandard/Sd.h>

#include <Library/DebugLib.h>

#include "MsdcDxe.h"

STATIC
EFI_STATUS
CardReset (
  IN EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru)
{
  EFI_SD_MMC_COMMAND_BLOCK Cmd;
  EFI_SD_MMC_STATUS_BLOCK StatusBlock;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET Packet;

  ZeroMem (&Cmd, sizeof (Cmd));
  ZeroMem (&StatusBlock, sizeof (StatusBlock));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &Cmd;
  Packet.SdMmcStatusBlk = &StatusBlock;

  Cmd.CommandIndex = SD_GO_IDLE_STATE;
  Cmd.CommandType  = SdMmcCommandTypeBc;

  return MsdcPassThru (PassThru, 0, &Packet, NULL);
}

STATIC
EFI_STATUS
CardSelect (
  IN EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru,
  IN UINT16                         Rca)
{
  EFI_SD_MMC_COMMAND_BLOCK Cmd;
  EFI_SD_MMC_STATUS_BLOCK StatusBlock;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET Packet;

  ZeroMem (&Cmd, sizeof (Cmd));
  ZeroMem (&StatusBlock, sizeof (StatusBlock));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &Cmd;
  Packet.SdMmcStatusBlk = &StatusBlock;

  Cmd.CommandIndex    = SD_SELECT_DESELECT_CARD;
  Cmd.CommandType     = SdMmcCommandTypeAc;
  Cmd.ResponseType    = SdMmcResponseTypeR1b;
  Cmd.CommandArgument = (UINT32)Rca << 16;

  return MsdcPassThru (PassThru, 0, &Packet, NULL);
}

STATIC
EFI_STATUS
CardSendStatus (
  IN EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru,
  IN UINT16                         Rca,
  OUT UINT32                       *DevStatus)
{
  EFI_SD_MMC_COMMAND_BLOCK Cmd;
  EFI_SD_MMC_STATUS_BLOCK StatusBlock;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET Packet;
  EFI_STATUS Status;

  ZeroMem (&Cmd, sizeof (Cmd));
  ZeroMem (&StatusBlock, sizeof (StatusBlock));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &Cmd;
  Packet.SdMmcStatusBlk = &StatusBlock;

  Cmd.CommandIndex    = SD_SEND_STATUS;
  Cmd.CommandType     = SdMmcCommandTypeAc;
  Cmd.ResponseType    = SdMmcResponseTypeR1;
  Cmd.CommandArgument = (UINT32)Rca << 16;

  Status = MsdcPassThru (PassThru, 0, &Packet, NULL);
  if (!EFI_ERROR (Status)) {
    *DevStatus = StatusBlock.Resp0;
  }

  return Status;
}

STATIC
EFI_STATUS
CardSendTuningBlock (
  IN EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru,
  IN UINT16                         Command)
{
  EFI_SD_MMC_COMMAND_BLOCK Cmd;
  EFI_SD_MMC_STATUS_BLOCK StatusBlock;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET Packet;
  EFI_STATUS Status;
  UINT8 TuningBlock[128];

  ZeroMem (&Cmd, sizeof (Cmd));
  ZeroMem (&StatusBlock, sizeof (StatusBlock));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &Cmd;
  Packet.SdMmcStatusBlk = &StatusBlock;

  Cmd.CommandIndex    = Command;
  Cmd.CommandType     = SdMmcCommandTypeAdtc;
  Cmd.ResponseType    = SdMmcResponseTypeR1;
  Cmd.CommandArgument = 0;

  Packet.InDataBuffer = TuningBlock;
  if (Command == EMMC_SEND_TUNING_BLOCK) {
    Packet.InTransferLength = sizeof (TuningBlock);
  } else {
    Packet.InTransferLength = 64;
  }

  Status = MsdcPassThru (PassThru, 0, &Packet, NULL);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return Status;
}

STATIC
UINT32
CardGetClockFrequency (
  IN MSDC_TIMING BusTiming)
{
  switch (BusTiming) {
    case TimingSdDs:
    case TimingUhsSdr12:
      return 25 * 1000 * 1000;
    case TimingSdHs:
    case TimingUhsSdr25:
    case TimingUhsDdr50:
      return 50 * 1000 * 1000;
    case TimingUhsSdr50:
      return 100 * 1000 * 1000;
    case TimingUhsSdr104:
      return 208 * 1000 * 1000;
    case TimingMmcLegacy:
      return 26 * 1000 * 1000;
    case TimingMmcSdr:
    case TimingMmcDdr:
      return 52 * 1000 * 1000;
    case TimingMmcHs200:
    case TimingMmcHs400:
      return 200 * 1000 * 1000;
  }

  return 0;
}

STATIC
EFI_STATUS
CardTuning (
  IN MSDC_CONTEXT *Ctx,
  IN UINT16        Command)
{
  EFI_STATUS Status;
  UINT64 RiseDelay = 0;
  UINT64 FallDelay = 0;
  MSDC_DELAY_PHASE FinalRiseDelay = {0};
  MSDC_DELAY_PHASE FinalFallDelay = {0};
  UINT8 FinalDelay;

  if ((gPlatformInfo.DataTune && gPlatformInfo.AsyncFifo) == FALSE) {
    return EFI_UNSUPPORTED;
  }

  MsdcHwSetDataSampleEdge (Ctx, TRUE);
  for (UINTN i = 0; i < gPlatformInfo.TuningStep[Ctx->Index]; i++) {
    MsdcHwSetCommandDelay (Ctx, i);
    MsdcHwSetDataDelay (Ctx, i);
    Status = CardSendTuningBlock (&Ctx->PassThru, Command);
    if (!EFI_ERROR (Status)) {
      RiseDelay |= (1ULL << i);
    }
  }

  FinalRiseDelay = MsdcHwGetBestDelay (Ctx, RiseDelay);
  if (FinalRiseDelay.MaxLen >= 12 || (FinalRiseDelay.Start == 0 && FinalRiseDelay.MaxLen >= 4)) {
    goto SkipFallDelay;
  }

  MsdcHwSetDataSampleEdge (Ctx, FALSE);
  for (UINTN i = 0; i < gPlatformInfo.TuningStep[Ctx->Index]; i++) {
    MsdcHwSetCommandDelay (Ctx, i);
    MsdcHwSetDataDelay (Ctx, i);
    Status = CardSendTuningBlock (&Ctx->PassThru, Command);
    if (!EFI_ERROR (Status)) {
      FallDelay |= (1ULL << i);
    }
  }

  FinalFallDelay = MsdcHwGetBestDelay (Ctx, FallDelay);

SkipFallDelay:
  if (FinalRiseDelay.MaxLen >= FinalFallDelay.MaxLen) {
    MsdcHwSetDataSampleEdge (Ctx, TRUE);
    FinalDelay = FinalRiseDelay.FinalPhase;
  } else {
    MsdcHwSetDataSampleEdge (Ctx, FALSE);
    FinalDelay = FinalFallDelay.FinalPhase;
  }

  MsdcHwSetCommandDelay (Ctx, FinalDelay);
  MsdcHwSetDataDelay (Ctx, FinalDelay);

  return FinalDelay == 0xFF ? EFI_DEVICE_ERROR : EFI_SUCCESS;
}

STATIC
EFI_STATUS
EmmcSendOpCond (
  IN EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru,
  IN OUT UINT32                    *Ocr)
{
  EFI_SD_MMC_COMMAND_BLOCK Cmd;
  EFI_SD_MMC_STATUS_BLOCK StatusBlock;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET Packet;
  EFI_STATUS Status;

  ZeroMem (&Cmd, sizeof (Cmd));
  ZeroMem (&StatusBlock, sizeof (StatusBlock));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &Cmd;
  Packet.SdMmcStatusBlk = &StatusBlock;

  Cmd.CommandIndex    = EMMC_SEND_OP_COND;
  Cmd.CommandType     = SdMmcCommandTypeBcr;
  Cmd.ResponseType    = SdMmcResponseTypeR3;
  Cmd.CommandArgument = *Ocr;

  Status = MsdcPassThru (PassThru, 0, &Packet, NULL);
  if (!EFI_ERROR (Status)) {
    *Ocr = StatusBlock.Resp0;
  }

  return Status;
}

STATIC
EFI_STATUS
EmmcGetAllCid (
  IN EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru)
{
  EFI_SD_MMC_COMMAND_BLOCK Cmd;
  EFI_SD_MMC_STATUS_BLOCK StatusBlock;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET Packet;

  ZeroMem (&Cmd, sizeof (Cmd));
  ZeroMem (&StatusBlock, sizeof (StatusBlock));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &Cmd;
  Packet.SdMmcStatusBlk = &StatusBlock;

  Cmd.CommandIndex    = EMMC_ALL_SEND_CID;
  Cmd.CommandType     = SdMmcCommandTypeBcr;
  Cmd.ResponseType    = SdMmcResponseTypeR2;
  Cmd.CommandArgument = 0;

  return MsdcPassThru (PassThru, 0, &Packet, NULL);
}

STATIC
EFI_STATUS
EmmcSetRca (
  IN EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru,
  IN UINT16                         Rca)
{
  EFI_SD_MMC_COMMAND_BLOCK Cmd;
  EFI_SD_MMC_STATUS_BLOCK StatusBlock;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET Packet;

  ZeroMem (&Cmd, sizeof (Cmd));
  ZeroMem (&StatusBlock, sizeof (StatusBlock));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &Cmd;
  Packet.SdMmcStatusBlk = &StatusBlock;

  Cmd.CommandIndex    = EMMC_SET_RELATIVE_ADDR;
  Cmd.CommandType     = SdMmcCommandTypeAc;
  Cmd.ResponseType    = SdMmcResponseTypeR1;
  Cmd.CommandArgument = (UINT32)Rca << 16;

  return MsdcPassThru (PassThru, 0, &Packet, NULL);
}

STATIC
EFI_STATUS
EmmcGetExtCsd (
  IN EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru,
  OUT EMMC_EXT_CSD                 *ExtCsd)
{
  EFI_SD_MMC_COMMAND_BLOCK Cmd;
  EFI_SD_MMC_STATUS_BLOCK StatusBlock;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET Packet;

  ZeroMem (&Cmd, sizeof (Cmd));
  ZeroMem (&StatusBlock, sizeof (StatusBlock));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &Cmd;
  Packet.SdMmcStatusBlk = &StatusBlock;

  Cmd.CommandIndex    = EMMC_SEND_EXT_CSD;
  Cmd.CommandType     = SdMmcCommandTypeAdtc;
  Cmd.ResponseType    = SdMmcResponseTypeR1;
  Cmd.CommandArgument = 0x00000000;

  Packet.InDataBuffer     = ExtCsd;
  Packet.InTransferLength = sizeof (EMMC_EXT_CSD);

  return MsdcPassThru (PassThru, 0, &Packet, NULL);
}

STATIC
EFI_STATUS
EmmcSwitch (
  IN EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru,
  IN UINT8                          Access,
  IN UINT8                          Index,
  IN UINT8                          Value,
  IN UINT8                          CmdSet)
{
  EFI_SD_MMC_COMMAND_BLOCK Cmd;
  EFI_SD_MMC_STATUS_BLOCK StatusBlock;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET Packet;

  ZeroMem (&Cmd, sizeof (Cmd));
  ZeroMem (&StatusBlock, sizeof (StatusBlock));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &Cmd;
  Packet.SdMmcStatusBlk = &StatusBlock;

  Cmd.CommandIndex    = EMMC_SWITCH;
  Cmd.CommandType     = SdMmcCommandTypeAc;
  Cmd.ResponseType    = SdMmcResponseTypeR1b;
  Cmd.CommandArgument = (Access << 24) | (Index << 16) | (Value << 8) | CmdSet;

  return MsdcPassThru (PassThru, 0, &Packet, NULL);
}

STATIC
MSDC_TIMING
EmmcGetBusTiming (
  IN EMMC_EXT_CSD *ExtCsd)
{
  if (ExtCsd->DeviceType & (BIT4 | BIT5)) {
    return TimingMmcHs200;
  } else if (ExtCsd->DeviceType & (BIT2 | BIT3)) {
    return TimingMmcDdr;
  } else if (ExtCsd->DeviceType & BIT1) {
    return TimingMmcSdr;
  } else {
    return TimingMmcLegacy;
  }
}

STATIC
EFI_STATUS
EmmcSwitchBusWidth (
  IN EFI_SD_MMC_PASS_THRU_PROTOCOL  *PassThru,
  IN UINT16                          Rca,
  IN MSDC_BUS_WIDTH                  BusWidth)
{
  EFI_STATUS Status;
  UINT32 DevStatus;
  UINT8 Value;

  switch (BusWidth) {
    case BusWidth8:
      Value = 0x02;
      break;
    case BusWidth4:
      Value = 0x01;
      break;
    case BusWidth1:
      Value = 0x00;
      break;
    default:
      return EFI_INVALID_PARAMETER;
  }

  Status = EmmcSwitch (PassThru, 0x03, OFFSET_OF (EMMC_EXT_CSD, BusWidth), Value, 0);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = CardSendStatus (PassThru, Rca, &DevStatus);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (DevStatus & BIT7) {
    return EFI_DEVICE_ERROR;
  }

  return MsdcHwSetBusWidth (MSDC_CONTEXT_FROM_THIS(PassThru), BusWidth);
}

STATIC
EFI_STATUS
EmmcSwitchBusTiming (
  IN EFI_SD_MMC_PASS_THRU_PROTOCOL  *PassThru,
  IN UINT16                          Rca,
  IN MSDC_TIMING                     Timing)
{
  EFI_STATUS Status;
  UINT32 DevStatus;
  UINT8 Value;

  switch (Timing) {
    case TimingMmcHs400:
      Value = 3;
      break;
    case TimingMmcHs200:
      Value = 2;
      break;
    case TimingMmcSdr:
    case TimingMmcDdr:
      Value = 1;
      break;
    default:
    case TimingMmcLegacy:
      Value = 0;
      break;
  }

  Status = EmmcSwitch (PassThru, 0x03, OFFSET_OF (EMMC_EXT_CSD, HsTiming), Value, 0);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = CardSendStatus (PassThru, Rca, &DevStatus);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (DevStatus & BIT7) {
    return EFI_DEVICE_ERROR;
  }

  return MsdcHwSetMclk (MSDC_CONTEXT_FROM_THIS(PassThru), CardGetClockFrequency (Timing), Timing);
}

STATIC
EFI_STATUS
EmmcSetBusMode (
  IN MSDC_CONTEXT *Ctx,
  IN UINT16        Rca)
{
  EFI_STATUS Status;
  EMMC_EXT_CSD ExtCsd;
  MSDC_TIMING BusTiming;

  Status = CardSelect (&Ctx->PassThru, Rca);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = EmmcGetExtCsd (&Ctx->PassThru, &ExtCsd);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  BusTiming = EmmcGetBusTiming (&ExtCsd);

  Status = EmmcSwitchBusWidth (&Ctx->PassThru, Rca, BusWidth8);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = EmmcSwitchBusTiming (&Ctx->PassThru, Rca, BusTiming);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (BusTiming == TimingMmcHs200) {
    Status = CardTuning (Ctx, EMMC_SEND_TUNING_BLOCK);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  DEBUG ((DEBUG_ERROR, "Switch EMMC Device to %a Mode\n", BusTiming == TimingMmcHs400 ? "HS400" : BusTiming == TimingMmcHs200 ? "HS200" : "HighSpeed"));

  return Status;
}

STATIC
EFI_STATUS
EmmcIdentification (
  IN MSDC_CONTEXT *Ctx)
{
  EFI_STATUS Status;
  UINT32 Ocr = 0;
  UINT32 Retry = 0;

  Status = PowerControl (Ctx->Index, TRUE, 3300000);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  MicroSecondDelay (20000);
  Status = MsdcHwSetMclk (Ctx, 400 * 1000, TimingMmcSdr);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = CardReset (&Ctx->PassThru);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  do {
    Status = EmmcSendOpCond (&Ctx->PassThru, &Ocr);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Ocr |= BIT30;

    if (Retry++ >= 100) {
      return EFI_DEVICE_ERROR;
    }

    MicroSecondDelay (10000);
  } while ((Ocr & BIT31) == 0);

  Status = EmmcGetAllCid (&Ctx->PassThru);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = EmmcSetRca (&Ctx->PassThru, 1);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Ctx->CardType = CardEmmc;
  DEBUG ((DEBUG_ERROR, "Found EMMC Device at Controller with Index %u\n", Ctx->Index));

  return EmmcSetBusMode (Ctx, 1);
}

STATIC
EFI_STATUS
SdCardSendOpCond (
  IN EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru,
  IN UINT16                         Rca,
  IN OUT UINT32                    *Ocr)
{
  EFI_SD_MMC_COMMAND_BLOCK Cmd;
  EFI_SD_MMC_STATUS_BLOCK StatusBlock;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET Packet;
  EFI_STATUS Status;

  ZeroMem (&Cmd, sizeof (Cmd));
  ZeroMem (&StatusBlock, sizeof (StatusBlock));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk = &Cmd;
  Packet.SdMmcStatusBlk = &StatusBlock;

  Cmd.CommandIndex    = SD_APP_CMD;
  Cmd.CommandType     = SdMmcCommandTypeAc;
  Cmd.ResponseType    = SdMmcResponseTypeR1;
  Cmd.CommandArgument = (UINT32)Rca << 16;

  Status = MsdcPassThru (PassThru, 0, &Packet, NULL);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Cmd.CommandIndex = SD_SEND_OP_COND;
  Cmd.CommandType  = SdMmcCommandTypeBcr;
  Cmd.ResponseType = SdMmcResponseTypeR3;
  Cmd.CommandArgument = *Ocr;

  Status = MsdcPassThru (PassThru, 0, &Packet, NULL);
  if (!EFI_ERROR (Status)) {
    *Ocr = StatusBlock.Resp0;
  }

  return Status;
}

STATIC
EFI_STATUS
SdCardVoltageSwitch (
  IN EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru)
{
  EFI_SD_MMC_COMMAND_BLOCK Cmd;
  EFI_SD_MMC_STATUS_BLOCK StatusBlock;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET Packet;

  ZeroMem (&Cmd, sizeof (Cmd));
  ZeroMem (&StatusBlock, sizeof (StatusBlock));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk = &Cmd;
  Packet.SdMmcStatusBlk = &StatusBlock;

  Cmd.CommandIndex = SD_VOLTAGE_SWITCH;
  Cmd.CommandType  = SdMmcCommandTypeAc;
  Cmd.ResponseType = SdMmcResponseTypeR1;
  Cmd.CommandArgument = 0;

  return MsdcPassThru (PassThru, 0, &Packet, NULL);
}

STATIC
EFI_STATUS
SdCardAllSendCid (
  IN EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru)
{
  EFI_SD_MMC_COMMAND_BLOCK Cmd;
  EFI_SD_MMC_STATUS_BLOCK StatusBlock;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET Packet;

  ZeroMem (&Cmd, sizeof (Cmd));
  ZeroMem (&StatusBlock, sizeof (StatusBlock));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk = &Cmd;
  Packet.SdMmcStatusBlk = &StatusBlock;

  Cmd.CommandIndex = SD_ALL_SEND_CID;
  Cmd.CommandType  = SdMmcCommandTypeBcr;
  Cmd.ResponseType = SdMmcResponseTypeR2;

  return MsdcPassThru (PassThru, 0, &Packet, NULL);
}

STATIC
EFI_STATUS
SdCardSetRca (
  IN EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru,
  OUT UINT16                       *Rca)
{
  EFI_SD_MMC_COMMAND_BLOCK Cmd;
  EFI_SD_MMC_STATUS_BLOCK StatusBlock;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET Packet;
  EFI_STATUS Status;

  ZeroMem (&Cmd, sizeof (Cmd));
  ZeroMem (&StatusBlock, sizeof (StatusBlock));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk = &Cmd;
  Packet.SdMmcStatusBlk = &StatusBlock;

  Cmd.CommandIndex = SD_SET_RELATIVE_ADDR;
  Cmd.CommandType  = SdMmcCommandTypeBcr;
  Cmd.ResponseType = SdMmcResponseTypeR6;

  Status = MsdcPassThru (PassThru, 0, &Packet, NULL);
  if (!EFI_ERROR (Status)) {
    *Rca = StatusBlock.Resp0 >> 16;
  }

  return Status;
}

STATIC
EFI_STATUS
SdCardSwitch (
  IN EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru,
  IN MSDC_TIMING                    BusTiming,
  IN UINT8                          CommandSystem,
  IN UINT8                          PowerLimit,
  IN BOOLEAN                        Mode,
  OUT UINT8                        *SwitchResp)
{
  EFI_SD_MMC_COMMAND_BLOCK Cmd;
  EFI_SD_MMC_STATUS_BLOCK StatusBlock;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET Packet;
  EFI_STATUS Status;
  UINT8 AccessMode;

  ZeroMem (&Cmd, sizeof (Cmd));
  ZeroMem (&StatusBlock, sizeof (StatusBlock));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk = &Cmd;
  Packet.SdMmcStatusBlk = &StatusBlock;

  Cmd.CommandIndex = SD_SWITCH_FUNC;
  Cmd.CommandType  = SdMmcCommandTypeAdtc;
  Cmd.ResponseType = SdMmcResponseTypeR1;

  switch (BusTiming) {
    case TimingUhsSdr12:
    case TimingSdDs:
      AccessMode = 0;
      break;
    case TimingUhsSdr25:
    case TimingSdHs:
      AccessMode = 0x1;
      break;
    case TimingUhsSdr50:
      AccessMode = 0x2;
      break;
    case TimingUhsDdr50:
      AccessMode = 0x4;
      break;
    case TimingUhsSdr104:
      AccessMode = 0x3;
      break;
    default:
      AccessMode = 0xF;
      break;
  }

  Cmd.CommandArgument = (AccessMode & 0xF) | ((CommandSystem & 0xF) << 4) | (0xF << 8) | ((PowerLimit & 0xF) << 12) | (Mode ? BIT31 : 0);

  Packet.InDataBuffer = SwitchResp;
  Packet.InTransferLength = 64;

  Status = MsdcPassThru (PassThru, 0, &Packet, NULL);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Mode) {
    if ((((AccessMode & 0xF) != 0xF) && ((SwitchResp[16] & 0xF) != AccessMode)) ||
        (((CommandSystem & 0xF) != 0xF) && (((SwitchResp[16] >> 4) & 0xF) != CommandSystem)) ||
        (((PowerLimit & 0xF) != 0xF) && (((SwitchResp[15] >> 4) & 0xF) != PowerLimit)))
    {
      return EFI_DEVICE_ERROR;
    }
  }

  return Status;
}

STATIC
MSDC_TIMING
SdCardGetBusTiming (
  IN UINT8  *SwitchResp,
  IN BOOLEAN Uhs)
{
  UINT8 SupportedTimings = SwitchResp[13];

  if (Uhs) {
    if (SupportedTimings & BIT3) {
      return TimingUhsSdr104;
    } else if (SupportedTimings & BIT4) {
      return TimingUhsDdr50;
    } else if (SupportedTimings & BIT2) {
      return TimingUhsSdr50;
    } else if (SupportedTimings & BIT1) {
      return TimingUhsSdr25;
    } else if (SupportedTimings & BIT0) {
      return TimingUhsSdr12;
    } else {
      return TimingSdDs;
    }
  } else {
    if (SupportedTimings & BIT1) {
      return TimingSdHs;
    } else {
      return TimingSdDs;
    }
  }
}

STATIC
EFI_STATUS
SdCardSetBusWidth (
  IN EFI_SD_MMC_PASS_THRU_PROTOCOL  *PassThru,
  IN UINT16                          Rca,
  IN MSDC_BUS_WIDTH                  BusWidth)
{
  EFI_SD_MMC_COMMAND_BLOCK Cmd;
  EFI_SD_MMC_STATUS_BLOCK StatusBlock;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET Packet;
  EFI_STATUS Status;
  UINT32 Value;

  ZeroMem (&Cmd, sizeof (Cmd));
  ZeroMem (&StatusBlock, sizeof (StatusBlock));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk = &Cmd;
  Packet.SdMmcStatusBlk = &StatusBlock;

  Cmd.CommandIndex    = SD_APP_CMD;
  Cmd.CommandType     = SdMmcCommandTypeAc;
  Cmd.ResponseType    = SdMmcResponseTypeR1;
  Cmd.CommandArgument = (UINT32)Rca << 16;

  Status = MsdcPassThru (PassThru, 0, &Packet, NULL);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Cmd.CommandIndex = SD_SET_BUS_WIDTH;
  Cmd.CommandType  = SdMmcCommandTypeAc;
  Cmd.ResponseType = SdMmcResponseTypeR1;

  switch (BusWidth) {
    default:
    case BusWidth8:
      return EFI_INVALID_PARAMETER;
    case BusWidth4:
      Value = 2;
      break;
    case BusWidth1:
      Value = 0;
      break;
  }

  Cmd.CommandArgument = Value;

  return MsdcPassThru (PassThru, 0, &Packet, NULL);
}

STATIC
EFI_STATUS
SdCardSwitchBusWidth (
  IN EFI_SD_MMC_PASS_THRU_PROTOCOL  *PassThru,
  IN UINT16                          Rca,
  IN MSDC_BUS_WIDTH                  BusWidth)
{
  EFI_STATUS Status;
  UINT32 DevStatus;

  if (BusWidth == BusWidth8) {
    return EFI_INVALID_PARAMETER;
  }

  Status = SdCardSetBusWidth (PassThru, Rca, BusWidth);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = CardSendStatus (PassThru, Rca, &DevStatus);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if ((DevStatus >> 16) != 0) {
    return EFI_DEVICE_ERROR;
  }

  return MsdcHwSetBusWidth (MSDC_CONTEXT_FROM_THIS(PassThru), BusWidth);
}

STATIC
EFI_STATUS
SdCardSetBusMode (
  IN MSDC_CONTEXT *Ctx,
  IN UINT16        Rca,
  IN BOOLEAN       S18A)
{
  EFI_STATUS Status;
  UINT8 SwitchResp[64];
  MSDC_TIMING BusTiming;

  ZeroMem (SwitchResp, sizeof(SwitchResp));

  Status = CardSelect (&Ctx->PassThru, Rca);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SdCardSwitch (&Ctx->PassThru, 0xF, 0xF, 0xF, FALSE, SwitchResp);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  BusTiming = SdCardGetBusTiming (SwitchResp, S18A);

  Status = SdCardSwitchBusWidth (&Ctx->PassThru, Rca, BusWidth4);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SdCardSwitch (&Ctx->PassThru, BusTiming, 0xF, 0xF, TRUE, SwitchResp);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = MsdcHwSetMclk (Ctx, CardGetClockFrequency (BusTiming), BusTiming);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (BusTiming == TimingUhsSdr104 || BusTiming == TimingUhsSdr50 || BusTiming == TimingUhsDdr50) {
    Status = CardTuning (Ctx, SD_SEND_TUNING_BLOCK);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  DEBUG ((DEBUG_ERROR, "Switch SD Device to %a Mode\n", BusTiming == TimingUhsSdr104 ? "SDR104" : BusTiming == TimingUhsDdr50 ? "DDR50" : BusTiming == TimingUhsSdr50 ? "SDR50" : BusTiming == TimingUhsSdr25 ? "SDR25" : BusTiming == TimingUhsSdr12 ? "SDR12" : BusTiming == TimingSdHs ? "High Speed" : "Default Speed"));

  return Status;
}

STATIC
EFI_STATUS
SdCardIdentification (
  IN MSDC_CONTEXT *Ctx)
{
  EFI_STATUS Status;
  UINT32 Ocr = 0;
  UINT32 Retry = 0;
  UINT16 Rca = 0;

  Status = PowerControl (Ctx->Index, TRUE, 3300000);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  MicroSecondDelay (20000);
  Status = MsdcHwSetMclk (Ctx, 400 * 1000, TimingSdDs);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = CardReset (&Ctx->PassThru);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SdCardSendOpCond (&Ctx->PassThru, 0, &Ocr);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  do {
    Ocr |= BIT24; // S18R
    Ocr |= BIT28; // XPC
    Ocr |= BIT30; // HCS

    Status = SdCardSendOpCond (&Ctx->PassThru, 0, &Ocr);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    if (Retry++ >= 100) {
      return EFI_DEVICE_ERROR;
    }

    MicroSecondDelay (10000);
  } while ((Ocr & BIT31) == 0);

  if (Ocr & BIT24) {
    Status = SdCardVoltageSwitch (&Ctx->PassThru);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = MsdcHwSetMclk (Ctx, 0, TimingSdDs);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = PowerControl (Ctx->Index, TRUE, 1800000);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    MicroSecondDelay (10000);
    Status = MsdcHwSetMclk (Ctx, 400 * 1000, TimingSdDs);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  Status = SdCardAllSendCid (&Ctx->PassThru);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SdCardSetRca (&Ctx->PassThru, &Rca);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Ctx->CardType = CardSd;
  DEBUG ((DEBUG_ERROR, "Found SD Device at Controller with Index %u\n", Ctx->Index));

  return SdCardSetBusMode (Ctx, Rca, (Ocr & BIT24) ? TRUE : FALSE);
}

MSDC_CARD_DETECT gCardDetectTable[] = {
  EmmcIdentification,
  SdCardIdentification,
  NULL
};