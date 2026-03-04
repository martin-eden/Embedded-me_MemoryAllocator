// Dynamic memory reserve/release

/*
  Author: Martin Eden
  Last mod.: 2026-03-04
*/

/*
  Implementation uses bitmap. So overhead is 1 / 8 of memory.
  Runtime is constant O(N).
*/

/*
  This is experimental module. Implementation is infested
  with debug output.
*/

#include <me_MemoryAllocator.h>

#include <me_BaseTypes.h>
#include <me_Bits.h>
#include <me_WorkMemory.h>
#include <me_AddrsegTools.h>
#include <me_WorkmemTools.h>

using namespace me_MemoryAllocator;

// Another lame constant I should move somewhere
const TUint_1 BitsInByte = 8;

// ( TMemoryAllocator

/*
  Free our data via stock free()
*/
TMemoryAllocator::~TMemoryAllocator()
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
TBool TMemoryAllocator::Init(
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
    We want to keep things simple

    Bit index is eight times more than byte index.

    So for 64 KiB max bit index is 512 Ki. For 2 KiB max is 16 Ki.

    512 Ki is more than 64 Ki TUint_2 can address.

    We want to keep bit index under 64 Ki (ATmega328 has no TUint_4
    support). That means maximum memory for heap is 8 KiB.
  */
  const TUint_2 MaxSize = TUint_2_Max / BitsInByte;

  if (Size > MaxSize) return false;

  IsReadyFlag = false;

  // Get memory for data
  // No memory for data? Return
  if (!Data.ResizeTo(Size)) return false;

  // Get memory for bitmap
  {
    TUint_2 BitmapSize = (Data.GetSize() + BitsInByte - 1) / BitsInByte;

    // No memory for bitmap? Release data and return.
    if (!Bitmap.ResizeTo(BitmapSize))
    {
      Data.Release();

      return false;
    }
  }

  IsReadyFlag = true;

  return true;
}

/*
  Return true when we are ready
*/
TBool TMemoryAllocator::IsReady()
{
  return IsReadyFlag;
}

/*
  Reserve block of given size

  Allocated block is always filled with zeroes.
*/
TBool TMemoryAllocator::Reserve(
  TAddressSegment * MemSeg,
  TUint_2 Size
)
{
  TUint_2 InsertIndex;

  // Zero size? Job done!
  if (Size == 0) return true;

  // Return if we have no idea where to insert
  if (!GetInsertIndex(&InsertIndex, Size)) return false;

  // So far <MemSeg.Addr> is just zero-based index
  MemSeg->Addr = InsertIndex;
  MemSeg->Size = Size;

  // Set all bits in bitmap for span
  MarkByteRange(*MemSeg);

  // Now <MemSeg.Addr> is real address
  MemSeg->Addr += Data.GetData().Addr;

  // Zero data (design requirement)
  me_WorkmemTools::ZeroMem(*MemSeg);

  return true;
}

/*
  Release block described by segment

  Released block may be filled with zeroes.
*/
TBool TMemoryAllocator::Release(
  TAddressSegment * MemSeg
)
{
  // Zero size? Job done!
  if (MemSeg->Size == 0)
  {
    MemSeg->Addr = 0;

    return true;
  }

  LastSegSize = MemSeg->Size;

  /*
    We're marking span as free in bitmap.
  */

  // Segment is not in our memory?
  if (!IsOurs(*MemSeg)) return false;

  // Zero data for security (optional)
  me_WorkmemTools::ZeroMem(*MemSeg);

  // Now <MemSeg.Addr> is zero-based index
  MemSeg->Addr -= Data.GetData().Addr;

  // Clear all bits in bitmap for span
  ClearByteRange(*MemSeg);

  // Yep, you're free to go
  MemSeg->Addr = 0;
  MemSeg->Size = 0;

  return true;
}

/*
  Get index of empty span in bitmap where we will allocate

  We have freedom where to place span in bitmap.
*/
TBool TMemoryAllocator::GetInsertIndex(
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
  TUint_2 Limit = Data.GetSize();
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
TUint_2 TMemoryAllocator::GetNextBusyIndex(
  TUint_2 StartIdx
)
{
  TUint_2 Limit = Data.GetSize();
  TUint_2 Cursor = StartIdx;

  for (Cursor = StartIdx; Cursor < Limit; ++Cursor)
    if (GetBit(Cursor))
      break;

  return Cursor;
}

/*
  [Internal] Set bits to given value in range
*/
void TMemoryAllocator::SetByteRange(
  TAddressSegment MemSeg,
  me_Bits::TBitValue BitValue
)
{
  TUint_2 StartBitIdx = MemSeg.Addr;
  TUint_2 EndBitIdx = StartBitIdx + MemSeg.Size - 1;

  for (TUint_2 Offset = StartBitIdx; Offset <= EndBitIdx; ++Offset)
    SetBit(Offset, BitValue);
}

/*
  [Internal] Set bitmap range bits
*/
void TMemoryAllocator::MarkByteRange(
  TAddressSegment MemSeg
)
{
  SetByteRange(MemSeg, 1);
}

/*
  [Internal] Clear bitmap range bits
*/
void TMemoryAllocator::ClearByteRange(
  TAddressSegment MemSeg
)
{
  SetByteRange(MemSeg, 0);
}

/*
  [Sanity] segment in our managed range?

  Empty segment is never ours.
*/
TBool TMemoryAllocator::IsOurs(
  TAddressSegment MemSeg
)
{
  return
    me_AddrsegTools::IsInside(MemSeg, Data.GetData());
}

/*
  Return bit value in bitmap's data

  Typically it's called from cycle, so no range checks.
*/
me_Bits::TBitValue TMemoryAllocator::GetBit(
  TUint_2 BitIndex
)
{
  /*
    We'll use raw functions without range checks
  */

  TAddress ByteAddress;
  TUnit ByteValue;
  TUint_1 BitOffset;
  me_Bits::TBitValue BitValue;

  ByteAddress = Bitmap.GetData().Addr + (BitIndex / BitsInByte);

  ByteValue = me_WorkMemory::Freetown::GetByteFrom(ByteAddress);

  BitOffset = BitIndex % BitsInByte;

  me_Bits::GetBit(&BitValue, ByteValue, BitOffset);

  return BitValue;
}

/*
  Set bit to given value in bitmap's data

  Same notice as for GetBit() - no checks here.
*/
void TMemoryAllocator::SetBit(
  TUint_2 BitIndex,
  me_Bits::TBitValue BitValue
)
{
  TAddress ByteAddress;
  TUnit ByteValue;
  TUint_1 BitOffset;

  ByteAddress = Bitmap.GetData().Addr + (BitIndex / BitsInByte);

  ByteValue = me_WorkMemory::Freetown::GetByteFrom(ByteAddress);

  BitOffset = BitIndex % BitsInByte;

  me_Bits::SetBitTo(&ByteValue, BitOffset, BitValue);

  me_WorkMemory::Freetown::SetByteAt(ByteAddress, ByteValue);
}

// ) TMemoryAllocator

/*
  Global instance
*/
me_MemoryAllocator::TMemoryAllocator HeapMem;

/*
  2024-10-11
  2024-10-12
  2024-10-13 Two insertion points, optimizing gap for next iteration
  2024-10-25 Using [me_Bits]
  2025-07-29
  2025-08-27
*/
