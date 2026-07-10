#include <Library/IoLib.h>

#include "MsdcDxe.h"

VOID
MsdcWrite (
  IN MSDC_CONTEXT *Ctx,
  IN UINT32        Offset,
  IN UINT32        Value)
{
  MmioWrite32 (Ctx->MmioBase + Offset, Value);
}

UINT32
MsdcRead (
  IN MSDC_CONTEXT *Ctx,
  IN UINT32        Offset)
{
  return MmioRead32 (Ctx->MmioBase + Offset);
}

UINT32
MsdcSetBits (
  IN MSDC_CONTEXT *Ctx,
  IN UINT32        Offset,
  IN UINT32        Bits)
{
  return MmioOr32(Ctx->MmioBase + Offset, Bits);
}

UINT32
MsdcClearBits (
  IN MSDC_CONTEXT *Ctx,
  IN UINT32        Offset,
  IN UINT32        Bits)
{
  return MmioAnd32(Ctx->MmioBase + Offset, ~Bits);
}

UINT32
MsdcClearSetBits (
  IN MSDC_CONTEXT *Ctx,
  IN UINT32        Offset,
  IN UINT32        Clear,
  IN UINT32        Set)
{
  return MmioAndThenOr32(Ctx->MmioBase + Offset, ~Clear, Set);
}

VOID
MsdcTopWrite (
  IN MSDC_CONTEXT *Ctx,
  IN UINT32        Offset,
  IN UINT32        Value)
{
  MmioWrite32 (Ctx->TopMmioBase + Offset, Value);
}

UINT32
MsdcTopRead (
  IN MSDC_CONTEXT *Ctx,
  IN UINT32        Offset)
{
  return MmioRead32 (Ctx->TopMmioBase + Offset);
}

UINT32
MsdcTopSetBits (
  IN MSDC_CONTEXT *Ctx,
  IN UINT32        Offset,
  IN UINT32        Bits)
{
  return MmioOr32(Ctx->TopMmioBase + Offset, Bits);
}

UINT32
MsdcTopClearBits (
  IN MSDC_CONTEXT *Ctx,
  IN UINT32        Offset,
  IN UINT32        Bits)
{
  return MmioAnd32(Ctx->TopMmioBase + Offset, ~Bits);
}

UINT32
MsdcTopClearSetBits (
  IN MSDC_CONTEXT *Ctx,
  IN UINT32        Offset,
  IN UINT32        Clear,
  IN UINT32        Set)
{
  return MmioAndThenOr32(Ctx->TopMmioBase + Offset, ~Clear, Set);
}