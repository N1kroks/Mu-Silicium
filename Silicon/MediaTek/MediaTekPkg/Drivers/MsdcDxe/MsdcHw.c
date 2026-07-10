#include <Library/BaseMemoryLib.h>
#include <Library/TimerLib.h>
#include <Library/IoLib.h>

#include <IndustryStandard/Sd.h>
#include <IndustryStandard/Emmc.h>

#include "MsdcDxe.h"

// Top Registers
#define EMMC_TOP_CONTROL 0x00
#define EMMC_TOP_CMD 0x04

// MSDC Registers
#define MSDC_CFG 0x00
#define MSDC_IOCON 0x04
#define MSDC_INT 0x0C
#define MSDC_INTEN 0x10
#define MSDC_FIFOCS 0x14
#define MSDC_TXDATA 0x018
#define MSDC_RXDATA 0x01C
#define MSDC_PATCH_BIT0 0xB0
#define MSDC_PATCH_BIT1 0xB4
#define MSDC_PATCH_BIT2 0xB8

// EMMC Registers
#define EMMC50_CFG2 0x21C

// SDC Registers
#define SDC_CFG 0x30
#define SDC_CMD 0x34
#define SDC_ARG 0x38
#define SDC_STS 0x3C
#define SDC_RESP0 0x40
#define SDC_RESP1 0x44
#define SDC_RESP2 0x48
#define SDC_RESP3 0x4C
#define SDC_BLK_NUM 0x50
#define SDC_ADV_CFG0 0x64
#define SDC_FIFO_CFG 0x228

// EMMC_TOP_CONTROL Bits
#define EMMC_TOP_CONTROL_PAD_RXDLY_SEL BIT0
#define EMMC_TOP_CONTROL_RXDLY2_SHIFT 2
#define EMMC_TOP_CONTROL_RXDLY2_MASK (0x1F << EMMC_TOP_CONTROL_RXDLY2_SHIFT)
#define EMMC_TOP_CONTROL_RXDLY_SHIFT 7
#define EMMC_TOP_CONTROL_RXDLY_MASK (0x1F << EMMC_TOP_CONTROL_RXDLY_SHIFT)
#define EMMC_TOP_CONTROL_RXDLY2_SEL BIT12
#define EMMC_TOP_CONTROL_RXDLY_SEL BIT13
#define EMMC_TOP_CONTROL_K_VALUE_SEL BIT14
#define EMMC_TOP_CONTROL_SDC_RX_ENCHANCE BIT15

// EMMC_TOP_CMD Bits
#define EMMC_TOP_CMD_RXDLY2_SHIFT 0
#define EMMC_TOP_CMD_RXDLY2_MASK (0x1F << EMMC_TOP_CMD_RXDLY2_SHIFT)
#define EMMC_TOP_CMD_RXDLY_SHIFT 5
#define EMMC_TOP_CMD_RXDLY_MASK (0x1F << EMMC_TOP_CMD_RXDLY_SHIFT)
#define EMMC_TOP_CMD_RD_RXDLY2_SEL   BIT10
#define EMMC_TOP_CMD_RD_RXDLY_SEL    BIT11

// MSDC_CFG Bits
#define MSDC_CFG_MODE BIT0
#define MSDC_CFG_CKPDN BIT1
#define MSDC_CFG_RST BIT2
#define MSDC_CFG_PIO BIT3
#define MSDC_CFG_CKSTB BIT7
#define MSDC_CFG_CKDIV_SHIFT 8
#define MSDC_CFG_CKDIV_MASK (0xFFF << MSDC_CFG_CKDIV_SHIFT)
#define MSDC_CFG_CKMOD_SHIFT 20
#define MSDC_CFG_CKMOD_MASK (3 << MSDC_CFG_CKMOD_SHIFT)
#define MSDC_CFG_HS400_CK_MODE_EXTRA BIT22

// MSDC_IOCON Bits
#define MSDC_IOCON_RSPL BIT1
#define MSDC_IOCON_DSPL BIT2
#define MSDC_IOCON_W_DSPL BIT8

// MSDC_INT bits
#define MSDC_INT_MMCIRQ BIT0
#define MSDC_INT_ACMDRDY BIT3
#define MSDC_INT_ACMDTMO BIT4
#define MSDC_INT_ACMDCRCERR BIT5
#define MSDC_INT_CMDRDY BIT8
#define MSDC_INT_CMDTMO BIT9
#define MSDC_INT_CMDCRCERR BIT10
#define MSDC_INT_XFER_COMPL BIT12
#define MSDC_INT_DATTMO BIT14
#define MSDC_INT_DATCRCERR BIT15

#define MSDC_INT_CMDSTS (MSDC_INT_CMDRDY | MSDC_INT_CMDTMO | MSDC_INT_CMDCRCERR | MSDC_INT_ACMDRDY | MSDC_INT_ACMDTMO | MSDC_INT_ACMDCRCERR)
#define MSDC_INT_DATSTS (MSDC_INT_DATTMO | MSDC_INT_DATCRCERR | MSDC_INT_XFER_COMPL)

// MSDC_FIFOCS Bits
#define MSDC_FIFOCS_CLR BIT31

// MSDC_PATCH_BIT0 Bits
#define MSDC_PATCH_BIT0_ODDSUPP BIT1
#define MSDC_PATCH_BIT0_DIS_WRMON BIT2
#define MSDC_PATCH_BIT0_DESCUP_SEL BIT6
#define MSDC_PATCH_BIT0_MSDC_DLY_SEL_SHIFT 10
#define MSDC_PATCH_BIT0_MSDC_DLY_SEL_MASK (0x1F << MSDC_PATCH_BIT0_MSDC_DLY_SEL_SHIFT)
#define MSDC_PATCH_BIT0_BUSYDLY_SHIFT 18
#define MSDC_PATCH_BIT0_BUSYDLY_MASK (0xF << MSDC_PATCH_BIT0_BUSYDLY_SHIFT)
#define MSDC_PATCH_BIT0_DECRCTMO BIT30

// MSDC_PATCH_BIT1 Bits
#define MSDC_PATCH_BIT1_WRDAT_CRC_TACNTR_SHIFT 0
#define MSDC_PATCH_BIT1_WRDAT_CRC_TACNTR_MASK (0x7 << MSDC_PATCH_BIT1_WRDAT_CRC_TACNTR_SHIFT)
#define MSDC_PATCH_BIT1_CMDTA_SHIFT 3
#define MSDC_PATCH_BIT1_CMDTA_MASK (0x7 << MSDC_PATCH_BIT1_CMDTA_SHIFT)
#define MSDC_PATCH_BIT1_BUSY_CHECK_SEL BIT7
#define MSDC_PATCH_BIT1_STOP_DLY_SHIFT 8
#define MSDC_PATCH_BIT1_STOP_DLY_MASK (0xF << MSDC_PATCH_BIT1_STOP_DLY_SHIFT)
#define MSDC_PATCH_BIT1_DDR_CMD_FIX_SEL BIT14
#define MSDC_PATCH_BIT1_SINGLE_BURST BIT16
#define MSDC_PATCH_BIT1_AUTO_SYNCST_CLR BIT19
#define MSDC_PATCH_BIT1_MARK_POP_WATER BIT20
#define MSDC_PATCH_BIT1_LP_DCM_EN BIT21
#define MSDC_PATCH_BIT1_AHB_GDMA_HCLK BIT23
#define MSDC_PATCH_BIT1_CLK_ENABLE 0xFF000000

// MSDC_PATCH_BIT2 Bits
#define MSDC_PATCH_BIT2_SUPPORT_64G BIT1
#define MSDC_PATCH_BIT2_RESPWAIT_SHIFT 2
#define MSDC_PATCH_BIT2_RESPWAIT_MASK (3 << MSDC_PATCH_BIT2_RESPWAIT_SHIFT)
#define MSDC_PATCH_BIT2_CFGRESP BIT15
#define MSDC_PATCH_BIT2_RESPSTSENSEL_SHIFT 16
#define MSDC_PATCH_BIT2_RESPSTSENSEL_MASK (0x7 << MSDC_PATCH_BIT2_RESPSTSENSEL_SHIFT)
#define MSDC_PATCH_BIT2_CFGCRCSTSEDGE BIT25
#define MSDC_PATCH_BIT2_CFGCRCSTS BIT28
#define MSDC_PATCH_BIT2_CRCSTSENSEL_SHIFT 29
#define MSDC_PATCH_BIT2_CRCSTSENSEL_MASK (0x7 << MSDC_PATCH_BIT2_CRCSTSENSEL_SHIFT)

// MSDC_PAD_TUNE bits
#define MSDC_PAD_TUNE_RD_SEL BIT13
#define MSDC_PAD_TUNE_RXDLYSEL BIT15
#define MSDC_PAD_TUNE_DATRRDLY_SHIFT 12
#define MSDC_PAD_TUNE_DATRRDLY_MASK (0x1F << MSDC_PAD_TUNE_DATRRDLY_SHIFT)
#define MSDC_PAD_TUNE_CMDRDLY_SHIFT 16
#define MSDC_PAD_TUNE_CMDRDLY_MASK (0x1F << MSDC_PAD_TUNE_CMDRDLY_SHIFT)
#define MSDC_PAD_TUNE_CMD_SEL BIT21

// EMMC50_CFG2 Bits
#define EMMC50_CFG2_AXI_SET_LEN_SHIFT 24
#define EMMC50_CFG2_AXI_SET_LEN_MASK (0xF << EMMC50_CFG2_AXI_SET_LEN_SHIFT)

// SDC_CFG bits
#define SDC_CFG_BUS_WIDTH_SHIFT 16
#define SDC_CFG_BUS_WIDTH_MASK (3 << SDC_CFG_BUS_WIDTH_SHIFT)
#define SDC_CFG_SDIO BIT19
#define SDC_CFG_SDIOIDE BIT20
#define SDC_CFG_DTOC_SHIFT 24
#define SDC_CFG_DTOC_MASK (0xFF << SDC_CFG_DTOC_SHIFT)

// SDC_CMD bits
#define SDC_CMD_RSP_TYPE_SHIFT 7
#define SDC_CMD_SINGLE_BLOCK BIT11
#define SDC_CMD_MULTIPLE_BLOCK BIT12
#define SDC_CMD_RW BIT13
#define SDC_CMD_STOP_CMD BIT14
#define SDC_CMD_BLK_SIZE_SHIFT 16
#define SDC_CMD_AUTO12 BIT28
#define SDC_CMD_VOLTAGE_SWITCH BIT30

// SDC_STS bits
#define SDC_STS_BUSY BIT0

// SDC_ADV_CFG0 Bits
#define SDC_ADV_CFG0_DAT1_IRQ_TRIGGER BIT19
#define SDC_ADV_CFG0_SDC_RX_ENCHANCE BIT20

// SDC_FIFO_CFG Bits
#define SDC_FIFO_CFG_WRVALIDSEL BIT24
#define SDC_FIFO_CFG_RDVALIDSEL BIT25

#define MSDC_FIFO_SIZE 128
#define MSDC_PAD_DELAY_HALF 32
#define MSDC_PAD_DELAY_FULL 64

typedef enum {
  ModeDivisor,
  ModeNoDivisor,
  ModeDdr,
  ModeHs400,
} MSDC_CLK_MODE;

STATIC
VOID
MsdcHwReset (
  IN MSDC_CONTEXT *Ctx)
{
  // Reset Controller
  MsdcSetBits (Ctx, MSDC_CFG, MSDC_CFG_RST);
  do {
    MicroSecondDelay(100);
  } while (MsdcRead (Ctx, MSDC_CFG) & MSDC_CFG_RST);

  // Clear FIFO
  MsdcSetBits (Ctx, MSDC_FIFOCS, MSDC_FIFOCS_CLR);
  do {
    MicroSecondDelay(100);
  } while (MsdcRead (Ctx, MSDC_FIFOCS) & MSDC_FIFOCS_CLR);

  // Clear interrupts
  MsdcWrite (Ctx, MSDC_INT, MsdcRead (Ctx, MSDC_INT));
}

EFI_STATUS
MsdcHwInit (
  IN MSDC_CONTEXT *Ctx)
{
  EFI_STATUS Status;

  ConfigureGpio (Ctx->Index);
  Status = ClockControl (Ctx->Index, TRUE);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  MsdcSetBits (Ctx, MSDC_CFG, MSDC_CFG_MODE | MSDC_CFG_CKPDN | MSDC_CFG_PIO);
  MsdcHwReset (Ctx);
  MsdcSetBits (Ctx, MSDC_INTEN, MSDC_INT_CMDSTS | MSDC_INT_DATSTS);

  if (gPlatformInfo.UseTop) {
    MsdcTopWrite (Ctx, EMMC_TOP_CONTROL, 0);
    MsdcTopWrite (Ctx, EMMC_TOP_CMD, 0);
  } else {
    MsdcWrite (Ctx, gPlatformInfo.MsdcPadTuneReg, 0);
  }

  MsdcWrite (Ctx, MSDC_IOCON, 0);

  MsdcWrite (Ctx, MSDC_PATCH_BIT0,
    MSDC_PATCH_BIT0_ODDSUPP |
    MSDC_PATCH_BIT0_DIS_WRMON |
    MSDC_PATCH_BIT0_DESCUP_SEL |
    (15 << MSDC_PATCH_BIT0_BUSYDLY_SHIFT) |
    MSDC_PATCH_BIT0_DECRCTMO |
    (1 << MSDC_PATCH_BIT0_MSDC_DLY_SEL_SHIFT));

  MsdcWrite (Ctx, MSDC_PATCH_BIT1,
    (1 << MSDC_PATCH_BIT1_WRDAT_CRC_TACNTR_SHIFT) |
    (1 << MSDC_PATCH_BIT1_CMDTA_SHIFT) |
    MSDC_PATCH_BIT1_DDR_CMD_FIX_SEL |
    MSDC_PATCH_BIT1_AUTO_SYNCST_CLR |
    MSDC_PATCH_BIT1_MARK_POP_WATER |
    MSDC_PATCH_BIT1_LP_DCM_EN |
    MSDC_PATCH_BIT1_AHB_GDMA_HCLK |
    MSDC_PATCH_BIT1_CLK_ENABLE);

  if (!((MsdcRead (Ctx, EMMC50_CFG2) & EMMC50_CFG2_AXI_SET_LEN_MASK) >> EMMC50_CFG2_AXI_SET_LEN_SHIFT)) {
    MsdcSetBits (Ctx, MSDC_PATCH_BIT1, MSDC_PATCH_BIT1_SINGLE_BURST);
  }

  if (gPlatformInfo.BusyCheck) {
    MsdcSetBits (Ctx, MSDC_PATCH_BIT1, MSDC_PATCH_BIT1_BUSY_CHECK_SEL);
  }

  if (gPlatformInfo.StopClkFix) {
    if (gPlatformInfo.StopDlySel) {
      MsdcSetBits (Ctx, MSDC_PATCH_BIT1, (gPlatformInfo.StopDlySel << MSDC_PATCH_BIT1_STOP_DLY_SHIFT));
    }

    MsdcClearBits (Ctx, SDC_FIFO_CFG, SDC_FIFO_CFG_WRVALIDSEL);
    MsdcClearBits (Ctx, SDC_FIFO_CFG, SDC_FIFO_CFG_RDVALIDSEL);
  }

  if (gPlatformInfo.AsyncFifo) {
    MsdcClearSetBits (Ctx, MSDC_PATCH_BIT2, MSDC_PATCH_BIT2_RESPWAIT_MASK, 3 << MSDC_PATCH_BIT2_RESPWAIT_SHIFT);
    MsdcClearSetBits (Ctx, MSDC_PATCH_BIT2, MSDC_PATCH_BIT2_CFGRESP, MSDC_PATCH_BIT2_CFGCRCSTS);

    if (gPlatformInfo.EnhanceRx) {
      MsdcClearSetBits (Ctx, MSDC_PATCH_BIT2,
        MSDC_PATCH_BIT2_RESPSTSENSEL_MASK | MSDC_PATCH_BIT2_CRCSTSENSEL_MASK,
        (2 << MSDC_PATCH_BIT2_RESPSTSENSEL_SHIFT) | (2 << MSDC_PATCH_BIT2_CRCSTSENSEL_SHIFT));
    } else if (gPlatformInfo.UseTop) {
      MsdcTopSetBits (Ctx, EMMC_TOP_CONTROL, EMMC_TOP_CONTROL_SDC_RX_ENCHANCE);
    } else {
      MsdcTopSetBits (Ctx, SDC_ADV_CFG0, SDC_ADV_CFG0_SDC_RX_ENCHANCE);
    }
  }

  if (gPlatformInfo.Support64g) {
    MsdcSetBits (Ctx, MSDC_PATCH_BIT2, MSDC_PATCH_BIT2_SUPPORT_64G);
  }

  if (gPlatformInfo.DataTune) {
    if (gPlatformInfo.UseTop) {
      MsdcTopSetBits (Ctx, EMMC_TOP_CONTROL, EMMC_TOP_CONTROL_RXDLY_SEL);
      MsdcTopClearBits (Ctx, EMMC_TOP_CONTROL, EMMC_TOP_CONTROL_K_VALUE_SEL);
      MsdcTopSetBits (Ctx, EMMC_TOP_CMD, EMMC_TOP_CMD_RD_RXDLY_SEL);
      if (gPlatformInfo.TuningStep[Ctx->Index] > MSDC_PAD_DELAY_HALF) {
        MsdcTopSetBits (Ctx, EMMC_TOP_CONTROL, EMMC_TOP_CONTROL_RXDLY2_SEL);
        MsdcTopSetBits (Ctx, EMMC_TOP_CMD, EMMC_TOP_CMD_RD_RXDLY2_SEL);
      }
    } else {
      MsdcSetBits (Ctx, gPlatformInfo.MsdcPadTuneReg, MSDC_PAD_TUNE_RD_SEL | MSDC_PAD_TUNE_CMD_SEL);
      if (gPlatformInfo.TuningStep[Ctx->Index] > MSDC_PAD_DELAY_HALF) {
        MsdcSetBits (Ctx, gPlatformInfo.MsdcPadTuneReg + 4, MSDC_PAD_TUNE_RD_SEL | MSDC_PAD_TUNE_CMD_SEL);
      }
    }
  } else {
    if (gPlatformInfo.UseTop) {
      MsdcTopSetBits (Ctx, EMMC_TOP_CONTROL, EMMC_TOP_CONTROL_PAD_RXDLY_SEL);
    } else {
      MsdcSetBits (Ctx, gPlatformInfo.MsdcPadTuneReg, MSDC_PAD_TUNE_RXDLYSEL);
    }
  }

  MsdcClearSetBits (Ctx, SDC_CFG, SDC_CFG_SDIOIDE, SDC_CFG_SDIO);
  MsdcSetBits (Ctx, SDC_ADV_CFG0, SDC_ADV_CFG0_DAT1_IRQ_TRIGGER);

  MsdcClearSetBits (Ctx, SDC_CFG, SDC_CFG_DTOC_MASK, 3 << SDC_CFG_DTOC_SHIFT);

  return Status;
}

EFI_STATUS
MsdcHwSetBusWidth (
  IN MSDC_CONTEXT  *Ctx,
  IN MSDC_BUS_WIDTH Width)
{
  if (Ctx == NULL || Width > BusWidth8) {
    return EFI_INVALID_PARAMETER;
  }

  MsdcClearSetBits (Ctx, SDC_CFG, SDC_CFG_BUS_WIDTH_MASK, Width << SDC_CFG_BUS_WIDTH_SHIFT);

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
MsdcHwSetTimeout (
  IN MSDC_CONTEXT *Ctx,
  IN UINT32 Sclk,
  IN UINT32 TimeoutNs,
  IN UINT32 TimeoutClks)
{
  UINT32 ClkNs;
  UINT32 Timeout;
  MSDC_CLK_MODE Mode;

  if (Ctx == NULL || Sclk == 0 || TimeoutNs == 0 || TimeoutClks == 0) {
    return EFI_INVALID_PARAMETER;
  }

  Mode = (MsdcRead (Ctx, MSDC_CFG) & MSDC_CFG_CKMOD_MASK) >> MSDC_CFG_CKMOD_SHIFT;

  ClkNs = 1000000000 / Sclk;
  Timeout = ((TimeoutNs + ClkNs - 1) / ClkNs) + TimeoutClks;
  Timeout = (Timeout + BIT20 - 1) >> 20;
  Timeout = Mode >= ModeDdr ? Timeout * 2 : Timeout;
  Timeout = Timeout > 1 ? Timeout - 1 : 0;

  return EFI_SUCCESS;
}

EFI_STATUS
MsdcHwSetMclk (
  IN MSDC_CONTEXT *Ctx,
  IN UINT32        Hz,
  IN MSDC_TIMING   Timing)
{
  EFI_STATUS Status;
  UINTN SourceHz;
  UINT32 Mode;
  UINT32 Divider;
  UINT32 Sclk;

  if (Ctx == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (Hz == 0) {
    MsdcClearBits (Ctx, MSDC_CFG, MSDC_CFG_CKPDN);
    return EFI_SUCCESS;
  }

  Status = GetSourceClockRate (Ctx->Index, &SourceHz);

  MsdcClearBits (Ctx, MSDC_CFG, MSDC_CFG_HS400_CK_MODE_EXTRA);
  if (Timing == TimingUhsDdr50 || Timing == TimingMmcDdr || Timing == TimingMmcHs400) {
    if (Timing == TimingMmcHs400) {
      Mode = ModeHs400;
    } else {
      Mode = ModeDdr;
    }

    if (Hz >= (SourceHz / 4)) {
      Divider = 0;
      Sclk = SourceHz / 4;
    } else {
      Divider = (SourceHz + (Hz * 4) - 1) / (Hz * 4);
      Sclk = (SourceHz / 4) / Divider;
      Divider /= 2;
    }

    if (Timing == TimingMmcHs400 && Hz >= (SourceHz / 2)) {
      MsdcSetBits (Ctx, MSDC_CFG, MSDC_CFG_HS400_CK_MODE_EXTRA);
      Sclk = SourceHz / 2;
      Divider = 0;
    }
  } else if (Hz >= SourceHz) {
    Mode = ModeNoDivisor;
    Divider = 0;
    Sclk = SourceHz;
  } else {
    Mode = ModeDivisor;
    if (Hz >= (SourceHz / 2)) {
      Divider = 0;
      Sclk = SourceHz / 2;
    } else {
      Divider = (SourceHz + (Hz * 4) - 1) / (Hz * 4);
      Sclk = (SourceHz / 4) / Divider;
    }
  }

  MsdcClearBits (Ctx, MSDC_CFG, MSDC_CFG_CKPDN);
  SourceClockControl (Ctx->Index, FALSE);
  MsdcClearSetBits (Ctx, MSDC_CFG,
    MSDC_CFG_CKDIV_MASK | MSDC_CFG_CKMOD_MASK,
    (Mode << MSDC_CFG_CKMOD_SHIFT) | (Divider << MSDC_CFG_CKDIV_SHIFT));
  SourceClockControl (Ctx->Index, TRUE);

  do {
    MicroSecondDelay(100);
  } while (!(MsdcRead (Ctx, MSDC_CFG) & MSDC_CFG_CKSTB));
  MsdcSetBits (Ctx, MSDC_CFG, MSDC_CFG_CKPDN);

  return MsdcHwSetTimeout(Ctx, Sclk, 100000000, 3 * 1048576);
}

STATIC
EFI_STATUS
MsdcHwPollInt (
  IN MSDC_CONTEXT *Ctx)
{
  UINT32 Status;
  UINT32 Timeout = 1000000;

  if (Ctx == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  while (TRUE) {
    Status = MsdcRead (Ctx, MSDC_INT);
    Status &= MSDC_INT_CMDSTS;
    if (Status) {
      break;
    }

    MicroSecondDelay (1);
    if (Timeout-- == 0) {
      return EFI_TIMEOUT;
    }
  }

  MsdcWrite (Ctx, MSDC_INT, Status);

  if (Status & MSDC_INT_CMDRDY) {
    return EFI_SUCCESS;
  }

  if (Status & (MSDC_INT_CMDTMO | MSDC_INT_ACMDTMO)) {
    return EFI_TIMEOUT;
  }

  if (Status & (MSDC_INT_CMDCRCERR | MSDC_INT_ACMDCRCERR)) {
    return EFI_CRC_ERROR;
  }

  return EFI_DEVICE_ERROR;
}

STATIC
UINT32
MsdcFifoRxBytes (
  IN MSDC_CONTEXT *Ctx)
{
  return MsdcRead (Ctx, MSDC_FIFOCS) & 0xFF;
}

STATIC
UINT32
MsdcFifoTxBytes (
  IN MSDC_CONTEXT *Ctx)
{
  return (MsdcRead (Ctx, MSDC_FIFOCS) >> 16) & 0xFF;
}

STATIC
EFI_STATUS
MsdcFifoRead (
  IN MSDC_CONTEXT *Ctx,
  OUT VOID        *Buf,
  IN UINT32        Len
  )
{
  UINT8 *Ptr = (UINT8 *)Buf;
  UINT32 *WidePtr = (UINT32 *)Buf;

  if (Ctx == NULL || (Buf == NULL && Len != 0)) {
    return EFI_INVALID_PARAMETER;
  }

  while (Len % 4) {
    *Ptr++ = MmioRead8 (Ctx->MmioBase + MSDC_RXDATA);
    Len--;
  }

  while (Len >= 4) {
    *WidePtr++ = MmioRead32 (Ctx->MmioBase + MSDC_RXDATA);
    Len -= 4;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
MsdcFifoWrite (
  IN MSDC_CONTEXT *Ctx,
  IN VOID         *Buf,
  IN UINT32        Len
  )
{
  UINT8 *Ptr = (UINT8 *)Buf;
  UINT32 *WidePtr = (UINT32 *)Buf;

  if (Ctx == NULL || (Buf == NULL && Len != 0)) {
    return EFI_INVALID_PARAMETER;
  }

  while (Len % 4) {
    MmioWrite8 (Ctx->MmioBase + MSDC_TXDATA, *Ptr++);
    Len--;
  }

  while (Len >= 4) {
    MmioWrite32 (Ctx->MmioBase + MSDC_TXDATA, *WidePtr++);
    Len -= 4;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
MsdcHwPioRead (
  IN MSDC_CONTEXT *Ctx,
  OUT VOID        *Buffer,
  IN UINT32        Bytes)
{
  UINT32 IntStatus;
  UINT32 Chunk;

  if (Ctx == NULL || (Buffer == NULL && Bytes != 0)) {
    return EFI_INVALID_PARAMETER;
  }

  while (TRUE) {
    IntStatus = MsdcRead (Ctx, MSDC_INT);
    IntStatus &= MSDC_INT_DATSTS;
    MsdcWrite (Ctx, MSDC_INT, IntStatus);

    if (IntStatus & MSDC_INT_DATCRCERR) {
      return EFI_CRC_ERROR;
    } else if (IntStatus & MSDC_INT_DATTMO) {
      return EFI_TIMEOUT;
    }

    Chunk = MIN(Bytes, MSDC_FIFO_SIZE);

    if (MsdcFifoRxBytes (Ctx) >= Chunk) {
      EFI_STATUS Status = MsdcFifoRead (Ctx, Buffer, Chunk);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      Buffer += Chunk;
      Bytes -= Chunk;
    }

    if (IntStatus & MSDC_INT_XFER_COMPL) {
      if (Bytes) {
        return EFI_ABORTED;
      }

      return EFI_SUCCESS;
    }
  }
}

STATIC
EFI_STATUS
MsdcHwPioWrite (
  IN MSDC_CONTEXT *Ctx,
  IN VOID         *Buffer,
  IN UINT32        Bytes)
{
  UINT32 IntStatus;
  UINT32 Chunk;

  if (Ctx == NULL || (Buffer == NULL && Bytes != 0)) {
    return EFI_INVALID_PARAMETER;
  }

  while (TRUE) {
    IntStatus = MsdcRead (Ctx, MSDC_INT);
    MsdcWrite (Ctx, MSDC_INT, IntStatus);

    if (IntStatus & MSDC_INT_DATCRCERR) {
      return EFI_CRC_ERROR;
    } else if (IntStatus & MSDC_INT_DATTMO) {
      return EFI_TIMEOUT;
    }

    Chunk = MIN(Bytes, MSDC_FIFO_SIZE);

    if ((MSDC_FIFO_SIZE - MsdcFifoTxBytes (Ctx)) >= Chunk) {
      EFI_STATUS Status = MsdcFifoWrite (Ctx, Buffer, Chunk);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      Buffer += Chunk;
      Bytes -= Chunk;
    }

    if (IntStatus & MSDC_INT_XFER_COMPL) {
      if (Bytes) {
        return EFI_ABORTED;
      }

      return EFI_SUCCESS;
    }
  }
}

EFI_STATUS
MsdcHwSendCmd (
  IN MSDC_CONTEXT                        *Ctx,
  IN EFI_SD_MMC_PASS_THRU_COMMAND_PACKET *Packet)
{
  EFI_STATUS Status;
  EFI_SD_MMC_COMMAND_BLOCK *Cmd = Packet->SdMmcCmdBlk;
  UINT32 Raw = Cmd->CommandIndex & 0x3F;
  UINT32 BlockCount = 0;

  if (Ctx == NULL || Packet == NULL || Packet->SdMmcCmdBlk == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (Cmd->CommandType != SdMmcCommandTypeBc) {
    switch (Cmd->ResponseType) {
      case SdMmcResponseTypeR1:
      case SdMmcResponseTypeR5:
      case SdMmcResponseTypeR6:
      case SdMmcResponseTypeR7:
        Raw |= (1 << SDC_CMD_RSP_TYPE_SHIFT);
        break;
      case SdMmcResponseTypeR2:
        Raw |= (2 << SDC_CMD_RSP_TYPE_SHIFT);
        break;
      case SdMmcResponseTypeR3:
        Raw |= (3 << SDC_CMD_RSP_TYPE_SHIFT);
        break;
      case SdMmcResponseTypeR4:
        Raw |= (4 << SDC_CMD_RSP_TYPE_SHIFT);
        break;
      case SdMmcResponseTypeR1b:
        Raw |= (7 << SDC_CMD_RSP_TYPE_SHIFT);
        break;
    }
  }

  switch (Cmd->CommandIndex) {
    case SD_READ_SINGLE_BLOCK:
      Raw |= SDC_CMD_SINGLE_BLOCK;
      BlockCount = 1;
      break;
    case SD_READ_MULTIPLE_BLOCK:
      Raw |= SDC_CMD_MULTIPLE_BLOCK;
      BlockCount = Packet->InTransferLength / 512;

      // Enable auto-sending of CMD12 for Sd Card Because SdDxe doesn't send it
      if (Ctx->CardType == CardSd) {
        Raw |= SDC_CMD_AUTO12;
      }
      break;
    case SD_WRITE_SINGLE_BLOCK:
      Raw |= SDC_CMD_RW | SDC_CMD_SINGLE_BLOCK;
      BlockCount = 1;
      break;
    case SD_WRITE_MULTIPLE_BLOCK:
      Raw |= SDC_CMD_RW | SDC_CMD_MULTIPLE_BLOCK;
      BlockCount = Packet->InTransferLength / 512;

      // Enable auto-sending of CMD12 for Sd Card Because SdDxe doesn't send it
      if (Ctx->CardType == CardSd) {
        Raw |= SDC_CMD_AUTO12;
      }
      break;
    case SD_STOP_TRANSMISSION:
      Raw |= SDC_CMD_STOP_CMD;
      break;
    case SD_VOLTAGE_SWITCH:
      Raw |= SDC_CMD_VOLTAGE_SWITCH;
      break;
    case SD_SWITCH_FUNC:
    case SD_SEND_STATUS:
    case EMMC_SEND_EXT_CSD:
      if (Cmd->CommandType == SdMmcCommandTypeAdtc) {
        Raw |= SDC_CMD_SINGLE_BLOCK;
        BlockCount = 1;
      }
      break;
  }

  if (BlockCount > 0) {
    Raw |= (Packet->InTransferLength < 512 ? Packet->InTransferLength : 512) << SDC_CMD_BLK_SIZE_SHIFT;
    MsdcWrite (Ctx, SDC_BLK_NUM, BlockCount);
  }

  do {
    MicroSecondDelay (100);
  } while (MsdcRead (Ctx, SDC_STS) & SDC_STS_BUSY);

  MsdcWrite (Ctx, SDC_ARG, Cmd->CommandArgument);
  MsdcWrite (Ctx, SDC_CMD, Raw);

  Status = MsdcHwPollInt (Ctx);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Cmd->CommandType != SdMmcCommandTypeBc) {
    if (Cmd->ResponseType == SdMmcResponseTypeR2) {
      Packet->SdMmcStatusBlk->Resp0 = MsdcRead (Ctx, SDC_RESP0);
      Packet->SdMmcStatusBlk->Resp1 = MsdcRead (Ctx, SDC_RESP1);
      Packet->SdMmcStatusBlk->Resp2 = MsdcRead (Ctx, SDC_RESP2);
      Packet->SdMmcStatusBlk->Resp3 = MsdcRead (Ctx, SDC_RESP3);

      CopyMem (&Packet->SdMmcStatusBlk->Resp0, (UINT8 *)&Packet->SdMmcStatusBlk->Resp0 + 1, 15);
    } else {
      Packet->SdMmcStatusBlk->Resp0 = MsdcRead (Ctx, SDC_RESP0);
    }
  }

  if (BlockCount > 0) {
    if (Raw & SDC_CMD_RW) {
      Status = MsdcHwPioWrite (Ctx, Packet->OutDataBuffer, Packet->OutTransferLength);
    } else {
      Status = MsdcHwPioRead (Ctx, Packet->InDataBuffer, Packet->InTransferLength);
    }
  }

  return Status;
}

EFI_STATUS
MsdcHwSetDataSampleEdge (
  IN MSDC_CONTEXT *Ctx,
  IN BOOLEAN       Rising)
{
  if (Ctx == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (Rising) {
    MsdcClearBits (Ctx, MSDC_IOCON, MSDC_IOCON_RSPL);
    MsdcClearBits (Ctx, MSDC_IOCON, MSDC_IOCON_DSPL);
    MsdcClearBits (Ctx, MSDC_IOCON, MSDC_IOCON_W_DSPL);
  } else {
    MsdcSetBits (Ctx, MSDC_IOCON, MSDC_IOCON_RSPL);
    MsdcSetBits (Ctx, MSDC_IOCON, MSDC_IOCON_DSPL);
    MsdcSetBits (Ctx, MSDC_IOCON, MSDC_IOCON_W_DSPL);
  }

  return EFI_SUCCESS;
}

EFI_STATUS
MsdcHwSetCommandDelay (
  IN MSDC_CONTEXT *Ctx,
  IN UINT32        Delay)
{
  if (Ctx == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (gPlatformInfo.UseTop) {
    if (Delay < MSDC_PAD_DELAY_HALF) {
      MsdcTopClearSetBits (Ctx, EMMC_TOP_CMD,
          EMMC_TOP_CMD_RXDLY_MASK | EMMC_TOP_CMD_RXDLY2_MASK,
          (Delay << EMMC_TOP_CMD_RXDLY_SHIFT));
    } else {
      MsdcTopClearSetBits (Ctx, EMMC_TOP_CMD,
          EMMC_TOP_CMD_RXDLY_MASK | EMMC_TOP_CMD_RXDLY2_MASK,
          ((MSDC_PAD_DELAY_HALF - 1) << EMMC_TOP_CMD_RXDLY_SHIFT) | ((Delay - MSDC_PAD_DELAY_HALF) << EMMC_TOP_CMD_RXDLY2_SHIFT));
    }
  } else {
    if (Delay < MSDC_PAD_DELAY_HALF) {
      MsdcClearSetBits (Ctx, gPlatformInfo.MsdcPadTuneReg, MSDC_PAD_TUNE_CMDRDLY_MASK, (Delay << MSDC_PAD_TUNE_CMDRDLY_SHIFT));
      MsdcClearSetBits (Ctx, gPlatformInfo.MsdcPadTuneReg + 4, MSDC_PAD_TUNE_CMDRDLY_MASK, 0);
    } else {
      MsdcClearSetBits (Ctx, gPlatformInfo.MsdcPadTuneReg, MSDC_PAD_TUNE_CMDRDLY_MASK, ((MSDC_PAD_DELAY_HALF - 1) << MSDC_PAD_TUNE_CMDRDLY_SHIFT));
      MsdcClearSetBits (Ctx, gPlatformInfo.MsdcPadTuneReg + 4, MSDC_PAD_TUNE_CMDRDLY_MASK, ((Delay - MSDC_PAD_DELAY_HALF) << MSDC_PAD_TUNE_CMDRDLY_SHIFT));
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
MsdcHwSetDataDelay (
  IN MSDC_CONTEXT *Ctx,
  IN UINT32        Delay)
{
  if (Ctx == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (gPlatformInfo.UseTop) {
    if (Delay < MSDC_PAD_DELAY_HALF) {
      MsdcTopClearSetBits (Ctx, EMMC_TOP_CONTROL,
          EMMC_TOP_CONTROL_RXDLY_MASK | EMMC_TOP_CONTROL_RXDLY2_MASK,
          (Delay << EMMC_TOP_CONTROL_RXDLY_SHIFT) | (Delay << EMMC_TOP_CONTROL_RXDLY2_SHIFT));
    } else {
      MsdcTopClearSetBits (Ctx, EMMC_TOP_CONTROL,
          EMMC_TOP_CONTROL_RXDLY_MASK | EMMC_TOP_CONTROL_RXDLY2_MASK,
          ((MSDC_PAD_DELAY_HALF - 1) << EMMC_TOP_CONTROL_RXDLY_SHIFT) | ((Delay - MSDC_PAD_DELAY_HALF) << EMMC_TOP_CONTROL_RXDLY2_SHIFT));
    }
  } else {
    if (Delay < MSDC_PAD_DELAY_HALF) {
      MsdcClearSetBits (Ctx, gPlatformInfo.MsdcPadTuneReg, MSDC_PAD_TUNE_DATRRDLY_MASK, (Delay << MSDC_PAD_TUNE_DATRRDLY_SHIFT));
      MsdcClearSetBits (Ctx, gPlatformInfo.MsdcPadTuneReg + 4, MSDC_PAD_TUNE_DATRRDLY_MASK, 0);
    } else {
      MsdcClearSetBits (Ctx, gPlatformInfo.MsdcPadTuneReg, MSDC_PAD_TUNE_DATRRDLY_MASK, ((MSDC_PAD_DELAY_HALF - 1) << MSDC_PAD_TUNE_DATRRDLY_SHIFT));
      MsdcClearSetBits (Ctx, gPlatformInfo.MsdcPadTuneReg + 4, MSDC_PAD_TUNE_DATRRDLY_MASK, ((Delay - MSDC_PAD_DELAY_HALF) << MSDC_PAD_TUNE_DATRRDLY_SHIFT));
    }
  }

  return EFI_SUCCESS;
}

STATIC
INTN
GetDelayLen(
  IN UINT64 Delay,
  IN UINT32 StartBit)
{
  UINT32 CurrentBit;

  for (UINT8 i = 0; i < (MSDC_PAD_DELAY_FULL - StartBit); i++) {
    CurrentBit = (StartBit + i) % MSDC_PAD_DELAY_FULL;
    if ((Delay & (1 << CurrentBit)) == 0) {
      return i;
    }
  }

  return MSDC_PAD_DELAY_FULL - StartBit;
}

MSDC_DELAY_PHASE
MsdcHwGetBestDelay(
  IN MSDC_CONTEXT *Ctx,
  IN UINT64 Delay)
{
  INTN Start = 0;
  INTN Len;
  INTN LenFinal;
  INTN StartFinal;
  MSDC_DELAY_PHASE DelayPhase = {0};
  UINT8 FinalPhase = 0xFF;

  if (Delay == 0) {
    DelayPhase.FinalPhase = FinalPhase;
    return DelayPhase;
  }

  while (Start < MSDC_PAD_DELAY_FULL) {
    Len = GetDelayLen (Delay, Start);
    if (LenFinal < Len) {
      StartFinal = Start;
      LenFinal = Len;
    }
    Start += Len ? Len : 1;
    if (!(Delay >> 32) && Len >= 12 && StartFinal < 4) {
      break;
    }
  }

  if (StartFinal == 0) {
    FinalPhase = (StartFinal + LenFinal / 3) % MSDC_PAD_DELAY_FULL;
  } else {
    FinalPhase = (StartFinal + LenFinal / 2) % MSDC_PAD_DELAY_FULL;
  }

  DelayPhase.MaxLen = LenFinal;
  DelayPhase.Start = StartFinal;
  DelayPhase.FinalPhase = FinalPhase;
  return DelayPhase;
}