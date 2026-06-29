#ifndef _SLOTLIB_H_
#define _SLOTLIB_H_

typedef enum {
  SlotUnknown,
  SlotA,
  SlotB,
} SLOT;

SLOT
GetActiveSlot (VOID);

#endif /* _SLOTLIB_H_ */
