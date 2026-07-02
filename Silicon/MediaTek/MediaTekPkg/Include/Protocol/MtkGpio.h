#ifndef _MTK_GPIO_H_
#define _MTK_GPIO_H_

typedef enum {
  DirIn  = 0,
  DirOut = 1
} MTK_GPIO_DIR;

typedef enum {
  BiasDisabled = 0,
  BiasPullUp   = BiasDisabled,
  BiasPullDown = 1
} MTK_GPIO_BIAS;

typedef enum {
  ResistanceR1R0_00 = 0,
  ResistanceR1R0_01 = 1,
  ResistanceR1R0_10 = 2,
  ResistanceR1R0_11 = 3,
} MTK_GPIO_RESISTANCE;

//
// Declare forward Reference to the GPIO Protocol
//
typedef struct _MTK_GPIO_PROTOCOL MTK_GPIO_PROTOCOL;

/**
  This Function Gets direction of the defined pin.

  @param[in]  Pin                          - The Pin.
  @param[out] Direction                    - The Direction of the Pin.
**/
typedef
EFI_STATUS
(EFIAPI *MTK_GPIO_GET_DIR) (
  IN  UINT32   Pin,
  OUT MTK_GPIO_DIR *Direction
  );

/**
  This Function Sets direction of the defined pin.

  @param[in] Pin                          - The Pin.
  @param[in] Direction                    - The Direction of the Pin.
**/
typedef
EFI_STATUS
(EFIAPI *MTK_GPIO_SET_DIR) (
  IN UINT32  Pin,
  IN MTK_GPIO_DIR Direction
  );

/**
  This Function Gets state of the defined pin.

  @param[in]  Pin                          - The Pin.
  @param[out] State                        - The State of the Pin.
**/
typedef
EFI_STATUS
(EFIAPI *MTK_GPIO_GET_STATE) (
  IN  UINT32   Pin,
  OUT BOOLEAN *State
  );

/**
  This Function Sets state of the defined pin.

  @param[in] Pin                          - The Pin.
  @param[in] State                        - The State of the Pin.
**/
typedef
EFI_STATUS
(EFIAPI *MTK_GPIO_SET_STATE) (
  IN UINT32  Pin,
  IN BOOLEAN State
  );

/**
  This Function Sets mode of the defined pin.

  @param[in] Pin                          - The Pin.
  @param[in] Mode                         - The Mode of the Pin.
**/
typedef
EFI_STATUS
(EFIAPI *MTK_GPIO_SET_MODE) (
  IN UINT32 Pin,
  IN UINT32 Mode
  );

/**
  This Function Sets Drive Strength of the defined pin.

  @param[in] Pin                          - The Pin.
  @param[in] DriveStrength                - The Drive Strength of the Pin.
**/
typedef
EFI_STATUS
(EFIAPI *MTK_GPIO_SET_DRIVE_STRENGTH) (
  IN UINT32 Pin,
  IN UINT32 DriveStrength
  );

/**
  This Function Sets Bias of the defined pin.

  @param[in] Pin                          - The Pin.
  @param[in] Bias                         - The Bias of the Pin.
  @param[in] Resistance                   - The Resistance of the Pin.
**/
typedef
EFI_STATUS
(EFIAPI *MTK_GPIO_SET_BIAS) (
  IN UINT32              Pin,
  IN MTK_GPIO_BIAS       Bias,
  IN MTK_GPIO_RESISTANCE Resistance
  );

//
// Define Protocol Functions
//
struct _MTK_GPIO_PROTOCOL {
  MTK_GPIO_GET_DIR   GetDir;
  MTK_GPIO_SET_DIR   SetDir;
  MTK_GPIO_GET_STATE GetState;
  MTK_GPIO_SET_STATE SetState;
  MTK_GPIO_SET_MODE  SetMode;
  MTK_GPIO_SET_DRIVE_STRENGTH SetDriveStrength;
  MTK_GPIO_SET_BIAS SetBias;
};

extern EFI_GUID gMediaTekGpioProtocolGuid;

#endif /* _MTK_GPIO_H_ */