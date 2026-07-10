#ifndef _MSDC_DXE_H_
#define _MSDC_DXE_H_

#include <Protocol/SdMmcPassThru.h>
#include <Library/MsdcImplLib.h>

#define MSDC_CONTEXT_SIGNATURE SIGNATURE_32 ('M', 'S', 'D', 'C')

#define MSDC_CONTEXT_FROM_THIS(a) \
    CR(a, MSDC_CONTEXT, PassThru, MSDC_CONTEXT_SIGNATURE)

typedef enum {
  CardUnknown,
  CardEmmc,
  CardSd,
} MSDC_CARD_TYPE;

typedef enum {
  BusWidth1 = 0,
  BusWidth4 = 1,
  BusWidth8 = 2,
} MSDC_BUS_WIDTH;

typedef enum {
  TimingSdDs,
  TimingSdHs,
  TimingUhsSdr12,
  TimingUhsSdr25,
  TimingUhsSdr50,
  TimingUhsDdr50,
  TimingUhsSdr104,
  TimingMmcLegacy,
  TimingMmcSdr,
  TimingMmcDdr,
  TimingMmcHs200,
  TimingMmcHs400,
} MSDC_TIMING;

typedef struct {
  UINT32 Signature;
  UINT8 Index;
  EFI_PHYSICAL_ADDRESS MmioBase;
  EFI_PHYSICAL_ADDRESS TopMmioBase;
  EFI_SD_MMC_PASS_THRU_PROTOCOL PassThru;
  EFI_HANDLE Handle;
  MSDC_CARD_TYPE CardType;
} MSDC_CONTEXT;

typedef struct {
  UINT8 MaxLen;
  UINT8 Start;
  UINT8 FinalPhase;
} MSDC_DELAY_PHASE;

typedef
EFI_STATUS
(*MSDC_CARD_DETECT) (
  IN MSDC_CONTEXT *Ctx
  );

extern MSDC_CARD_DETECT gCardDetectTable[];

EFI_STATUS
MsdcPassThru (
  IN EFI_SD_MMC_PASS_THRU_PROTOCOL           *This,
  IN UINT8                                    Slot,
  IN OUT EFI_SD_MMC_PASS_THRU_COMMAND_PACKET *Packet,
  IN EFI_EVENT                                Event
  );

EFI_STATUS
MsdcHwInit (
  IN MSDC_CONTEXT *Ctx
  );

EFI_STATUS
MsdcHwSetBusWidth (
  IN MSDC_CONTEXT  *Ctx,
  IN MSDC_BUS_WIDTH Width
  );

EFI_STATUS
MsdcHwSetMclk (
  IN MSDC_CONTEXT *Ctx,
  IN UINT32        Hz,
  IN MSDC_TIMING   Timing
  );

EFI_STATUS
MsdcHwSendCmd (
  IN MSDC_CONTEXT                        *Ctx,
  IN EFI_SD_MMC_PASS_THRU_COMMAND_PACKET *Packet
  );

EFI_STATUS
MsdcHwSetDataSampleEdge (
  IN MSDC_CONTEXT *Ctx,
  IN BOOLEAN       Rising
  );

EFI_STATUS
MsdcHwSetCommandDelay (
  IN MSDC_CONTEXT *Ctx,
  IN UINT32        Delay
  );

EFI_STATUS
MsdcHwSetDataDelay (
  IN MSDC_CONTEXT *Ctx,
  IN UINT32        Delay
  );

MSDC_DELAY_PHASE
MsdcHwGetBestDelay(
  IN MSDC_CONTEXT *Ctx,
  IN UINT64 Delay);

VOID
MsdcWrite (
  IN MSDC_CONTEXT *Ctx,
  IN UINT32        Offset,
  IN UINT32        Value
  );

UINT32
MsdcRead (
  IN MSDC_CONTEXT *Ctx,
  IN UINT32        Offset
  );

UINT32
MsdcSetBits (
  IN MSDC_CONTEXT *Ctx,
  IN UINT32        Offset,
  IN UINT32        Bits
  );

UINT32
MsdcClearBits (
  IN MSDC_CONTEXT *Ctx,
  IN UINT32        Offset,
  IN UINT32        Bits
  );

UINT32
MsdcClearSetBits (
  IN MSDC_CONTEXT *Ctx,
  IN UINT32        Offset,
  IN UINT32        Clear,
  IN UINT32        Set
  );

VOID
MsdcTopWrite (
  IN MSDC_CONTEXT *Ctx,
  IN UINT32        Offset,
  IN UINT32        Value
  );

UINT32
MsdcTopRead (
  IN MSDC_CONTEXT *Ctx,
  IN UINT32        Offset
  );


UINT32
MsdcTopSetBits (
  IN MSDC_CONTEXT *Ctx,
  IN UINT32        Offset,
  IN UINT32        Bits
  );

UINT32
MsdcTopClearBits (
  IN MSDC_CONTEXT *Ctx,
  IN UINT32        Offset,
  IN UINT32        Bits
  );

UINT32
MsdcTopClearSetBits (
  IN MSDC_CONTEXT *Ctx,
  IN UINT32        Offset,
  IN UINT32        Clear,
  IN UINT32        Set
  );

#endif /* _MSDC_DXE_H_ */