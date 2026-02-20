// [me_Heap] Dynamic memory management

/*
  Author: Martin Eden
  Last mod.: 2026-02-20
*/

/*
  Implementation uses bitmap. So overhead is (N / 8) for managing
  (N) bytes of memory. Runtime is constant O(N).
*/

/*
  This is experimental module. Implementation is infested
  with debug output.
*/

#include <me_Heap.h>

#include <me_BaseTypes.h>
#include <me_Bits.h>
#include <me_WorkMemory.h>
#include <me_AddrsegTools.h>
#include <me_WorkmemTools.h>
#include <me_ProgramMemory.h>

#include <me_Console.h> // [Debug]

using namespace me_Heap;

// Another lame constant I should move somewhere
const TUint_1 BitsInByte = 8;

// ( THeap

/*
  Free our data via stock free()
*/
THeap::~THeap()
{
  /*
    If we're alive via global (then we shouldn't die but anyway)
    clients should call "<OurGlobalName>.IsReady()".

    If it returns "false" they should use stock free().
  */
  IsReadyFlag = false;
}

/*
  Allocate memory and bitmap for given memory size

  This implementation can't allocate more than 8 KiB memory.
*/
TBool THeap::Init(
  TUint_2 Size
)
{
  /*
    We'll allocate <Size> bytes via stock malloc().
    And then <Size> / 8 bytes for bitmap.
    If we'll not fail, we'll return true.

    Actually we don't care about memory segment data that we
    will be subdividing. We'll store it in our class just for
    caller's convenience. What is ours is bitmap for that segment.
  */

  /*
    We want to keep things simple. Bit index is eight times more
    than byte index. So for 64Ki max bit index is 512Ki.

    That's outside of UInt_2 range that I'm trying to stay
    (unless absolutely necessary or unless I really want to.).

    In this case my system having 2Ki memory. Maximum this code
    can allocate is 8Ki. I'm not going to make deviations and
    write code that can allocate 64Ki. Or 4Gi. Or 4Gi*4Gi.
  */
  const TUint_2 MaxSize = 0xFFFF / BitsInByte;

  if (Size > MaxSize)
    return false;

  IsReadyFlag = false;

  // Get memory for data
  // No memory for data? Return
  if (!HeapMem.ResizeTo(Size))
    return false;

  // Get memory for bitmap
  {
    TUint_2 BitmapSize = (HeapMem.GetSize() + BitsInByte - 1) / BitsInByte;

    // No memory for bitmap? Release data and return.
    if (!Bitmap.ResizeTo(BitmapSize))
    {
      HeapMem.Release();
      return false;
    }
  }

  IsReadyFlag = true;

  return true;
}

/*
  Return true when we are ready
*/
TBool THeap::IsReady()
{
  return IsReadyFlag;
}

/*
  Reserve block of given size

  Allocated block is always filled with zeroes.
*/
TBool THeap::Reserve(
  TAddressSegment * MemSeg,
  TUint_2 Size
)
{
  // Zero size? Job done!
  if (Size == 0)
    return true;

  TUint_2 InsertIndex;

  // No idea where to place it? Return
  if (!GetInsertIndex(&InsertIndex, Size))
  {
    Console.PrintProgmem(M_AsProgmemSeg("[Heap] Failed to reserve."));
    return false;
  }

  // So far <MemSeg.Addr> is just zero-based index
  MemSeg->Addr = InsertIndex;
  MemSeg->Size = Size;

  //* [Sanity] All bits for span in bitmap must be clear (span is free)
  if (!RangeIsSolid(*MemSeg, 0))
  {
    Console.PrintProgmem(M_AsProgmemSeg("[Heap] Span is not free."));
    return false;
  }
  //*/

  // Set all bits in bitmap for span
  MarkRange(*MemSeg);

  // Now <MemSeg.Addr> is real address
  MemSeg->Addr = MemSeg->Addr + HeapMem.GetData().Addr;

  // Zero data (design requirement)
  me_WorkmemTools::ZeroMem(*MemSeg);

  Console.WriteProgmem(M_AsProgmemSeg("[Heap] Reserve ( Addr"));
  Console.Print(MemSeg->Addr);
  Console.Write("Size");
  Console.Print(MemSeg->Size);
  Console.Write(")");
  Console.EndLine();

  return true;
}

/*
  Release block described by segment

  Released block may be filled with zeroes.
*/
TBool THeap::Release(
  TAddressSegment * MemSeg
)
{
  // Zero size? Job done!
  if (MemSeg->Size == 0)
  {
    MemSeg->Addr = 0;
    return false;
  }

  Console.WriteProgmem(M_AsProgmemSeg("[Heap] Release ( Addr"));
  Console.Print(MemSeg->Addr);
  Console.Write("Size");
  Console.Print(MemSeg->Size);
  Console.Write(")");
  Console.EndLine();

  LastSegSize = MemSeg->Size;

  /*
    We're marking span as free in bitmap.
  */

  // Segment is not in our memory?
  if (!IsOurs(*MemSeg))
  {
    Console.PrintProgmem(M_AsProgmemSeg("[Heap] Not ours."));
    return false;
  }

  // Zero data for security (optional)
  me_WorkmemTools::ZeroMem(*MemSeg);

  // Now <MemSeg.Addr> is zero-based index
  MemSeg->Addr = MemSeg->Addr - HeapMem.GetData().Addr;

  //* [Sanity] All bits for span in bitmap must be set (span is used)
  if (!RangeIsSolid(*MemSeg, 1))
  {
    Console.PrintProgmem(M_AsProgmemSeg("[Heap] Span is not solid."));
    return false;
  }
  //*/

  // Clear all bits in bitmap for span
  ClearRange(*MemSeg);

  // Yep, you're free to go
  MemSeg->Addr = 0;
  MemSeg->Size = 0;

  return true;
}

/*
  Get index of empty span in bitmap where we will allocate

  We have freedom where to place span in bitmap.
*/
TBool THeap::GetInsertIndex(
  TUint_2 * Index,
  TUint_2 SegSize
)
{
  /*
    Below is best-fit algorithm.

    But when we found best segment, we'll attach our span to
    it's start or to it's end. We'll attach span to the end if
    current segment size is less than segment size from previous
    call of us. (Current segment size is less than size of
    previously allocated segment.)
  */

  TUint_2 Cursor = 0;
  TUint_2 Limit = HeapMem.GetSize();
  TUint_2 BestIndex = 0xFFFF;
  TUint_2 BestDelta = 0xFFFF;
  TUint_2 BestSpanLen = 0xFFFF;

  TUint_2 NextBusy;
  TUint_2 SpanLen;
  TUint_2 Delta; // Remaining gap
  do
  {
    NextBusy = GetNextBusyIndex(Cursor);

    SpanLen = NextBusy - Cursor;

    if (SpanLen >= SegSize)
    {
      Delta = SpanLen - SegSize;

      if (Delta < BestDelta)
      {
        BestIndex = Cursor;
        BestDelta = Delta;
        BestSpanLen = SpanLen;
      }
    }

    Cursor = NextBusy;

    ++Cursor;
  } while (Cursor < Limit);

  if (BestIndex < Limit)
  {
    TBool AttachToRight = (SegSize < LastSegSize);

    if (AttachToRight)
      BestIndex = BestIndex + (BestSpanLen - SegSize);

    LastSegSize = SegSize;

    *Index = BestIndex;

    return true;
  }

  return false;
}

/*
  Find nearest busy byte to the right
*/
TUint_2 THeap::GetNextBusyIndex(
  TUint_2 StartIdx
)
{
  TUint_2 Limit = HeapMem.GetSize();
  TUint_2 Cursor = StartIdx;

  for (Cursor = StartIdx; Cursor < Limit; ++Cursor)
    if (GetBit(Cursor))
      break;

  return Cursor;
}

/*
  [Internal] Set bits to given value in range
*/
void THeap::SetRange(
  TAddressSegment MemSeg,
  TUint_1 BitsValue
)
{
  TUint_2 StartBitIdx = MemSeg.Addr;
  TUint_2 EndBitIdx = StartBitIdx + MemSeg.Size - 1;

  for (TUint_2 Offset = StartBitIdx; Offset <= EndBitIdx; ++Offset)
    SetBit(Offset, BitsValue);
}

/*
  [Internal] Set bitmap range bits
*/
void THeap::MarkRange(
  TAddressSegment MemSeg
)
{
  SetRange(MemSeg, 1);
}

/*
  [Internal] Clear bitmap range bits
*/
void THeap::ClearRange(
  TAddressSegment MemSeg
)
{
  SetRange(MemSeg, 0);
}

/*
  [Sanity] segment in our managed range?

  Empty segment is never ours.
*/
TBool THeap::IsOurs(
  TAddressSegment MemSeg
)
{
  return
    me_AddrsegTools::IsInside(MemSeg, HeapMem.GetData());
}

/*
  [Internal] Bits in range have same given value?

  I.e. all bytes in range are marked as used/free in bitmap.
*/
TBool THeap::RangeIsSolid(
  TAddressSegment MemSeg,
  TUint_1 BitsValue
)
{
  TUint_2 StartBitIdx = MemSeg.Addr;
  TUint_2 EndBitIdx = StartBitIdx + MemSeg.Size - 1;

  for (TUint_2 Offset = StartBitIdx; Offset < EndBitIdx; ++Offset)
    if (GetBit(Offset) != BitsValue)
      return false;

  return true;
}

/*
  Return bit value in bitmap's data

  Typically it's called from cycle, so no range checks.
*/
TUint_1 THeap::GetBit(
  TUint_2 BitIndex
)
{
  /*
    We'll use raw functions without range checks
  */

  TAddress ByteAddress;
  TUnit ByteValue;
  TUint_1 BitOffset;
  TUint_1 BitValue;

  ByteAddress = Bitmap.GetData().Addr + (BitIndex / BitsInByte);

  ByteValue = me_WorkMemory::Freetown::GetByteFrom(ByteAddress);

  BitOffset = BitIndex % BitsInByte;

  me_Bits::Freetown::GetBit(&BitValue, ByteValue, BitOffset);

  return BitValue;
}

/*
  Set bit to given value in bitmap's data

  Same notice as for GetBit() - no checks here.
*/
void THeap::SetBit(
  TUint_2 BitIndex,
  TUint_1 BitValue
)
{
  TAddress ByteAddress;
  TUnit ByteValue;
  TUint_1 BitOffset;

  ByteAddress = Bitmap.GetData().Addr + (BitIndex / BitsInByte);

  ByteValue = me_WorkMemory::Freetown::GetByteFrom(ByteAddress);

  BitOffset = BitIndex % BitsInByte;

  if (BitValue == 1)
    me_Bits::Freetown::SetBit(&ByteValue, BitOffset);
  else
    me_Bits::Freetown::ClearBit(&ByteValue, BitOffset);

  me_WorkMemory::Freetown::SetByteAt(ByteAddress, ByteValue);
}

// ) THeap

/*
  Global instance
*/
me_Heap::THeap Heap;

/*
  2024-10-11
  2024-10-12
  2024-10-13 Two insertion points, optimizing gap for next iteration
  2024-10-25 Using [me_Bits]
  2025-07-29
  2025-08-27
*/
