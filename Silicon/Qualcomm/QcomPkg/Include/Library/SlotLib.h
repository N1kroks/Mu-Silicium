#ifndef _SLOTLIB_H_
#define _SLOTLIB_H_

typedef enum {
  SlotUnknown,
  SlotA,
  SlotB,
} SLOT;

BOOLEAN
HasMultipleSlots (
  VOID
  );

SLOT
GetActiveSlot (
  VOID
  );

#endif /* _SLOTLIB_H_ */
