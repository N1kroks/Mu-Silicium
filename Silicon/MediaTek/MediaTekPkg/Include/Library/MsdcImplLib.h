#ifndef _MSDC_IMPL_LIB_H_
#define _MSDC_IMPL_LIB_H_

typedef struct {
  UINT8   NumberOfHosts;
  BOOLEAN UseTop;
  UINT32  MsdcPadTuneReg;
  UINT32  TuningStep[2];
  BOOLEAN AsyncFifo;
  BOOLEAN BusyCheck;
  BOOLEAN StopClkFix;
  UINT32  StopDlySel;
  BOOLEAN EnhanceRx;
  BOOLEAN Support64g;
  BOOLEAN DataTune;
} MSDC_PLATFORM_INFO;

EFI_STATUS
GetSourceClockRate (
  UINT32 Index,
  UINTN *Hz
  );

EFI_STATUS
SourceClockControl (
  UINT32 Index,
  BOOLEAN Enable
  );

EFI_STATUS
ClockControl (
  UINT32 Index,
  BOOLEAN Enable
  );

EFI_STATUS
PowerControl (
  UINT32 Index,
  BOOLEAN Enable,
  UINT32 VoltageLevel
  );

VOID
ConfigureGpio (
  UINT32 Index
  );

extern MSDC_PLATFORM_INFO gPlatformInfo;

#endif /* _MSDC_IMPL_LIB_H_ */