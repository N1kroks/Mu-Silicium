#ifndef _GPIO_IMPL_LIB_H_
#define _GPIO_IMPL_LIB_H_

typedef struct {
  UINT32 SetOffset;
  UINT32 ResetOffset;
  UINT32 DirOffset;
  UINT32 DataOutOffset;
  UINT32 DataInOffset;
  UINT32 ModeOffset;
  UINT32 MaxPin;
} MTK_GPIO_PLATFORM_INFO;

typedef struct {
  UINTN  Region;
  UINT32 Offset;
  UINT32 Mask;
  UINT32 Shift;
} MTK_GPIO_DRIVE_STRENGTH_TABLE;

typedef struct {
  UINT32 Pin;
  UINTN  Region;
  UINT32 Offset;
  UINT32 Mask;
  UINT32 Shift;
} MTK_GPIO_BIAS_TABLE;

typedef struct {
  UINT32 Pin;
  UINTN  Region;
  UINT32 OffsetR1;
  UINT32 OffsetR0;
  UINT32 Mask;
  UINT32 Shift;
} MTK_GPIO_RESISTANCE_TABLE;

extern MTK_GPIO_PLATFORM_INFO gPlatformInfo;
extern MTK_GPIO_DRIVE_STRENGTH_TABLE gDriveStrengthTable[];

extern MTK_GPIO_BIAS_TABLE gBiasTable[];
extern UINTN gBiasTableCount;

extern MTK_GPIO_RESISTANCE_TABLE gResistanceTable[];
extern UINTN gResistanceTableCount;

extern EFI_PHYSICAL_ADDRESS gPinctrlBankRegions[];
extern UINTN gPinctrlBankRegionCount;

#endif /* _GPIO_IMPL_LIB_H_ */