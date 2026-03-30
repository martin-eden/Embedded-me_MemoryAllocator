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
  Use provided memory segment as container for further allocations
*/
TBool TMemoryAllocator::Init(
  TAddressSegment Container
)
{
  /*
    We got memory segment to subdivide named <Container>

    We'll reserve 1/8 of it for bitmap.

    We want to keep bit index under 64 Ki. That means maximum data
    memory is 8 KiB.
  */
  const TUint_2 MaxSize = TUint_2_Max / BitsInByte;

  TUint_2 BitmapSize;
  TUint_2 DataSize;
  TUint_2 PrevDataSize;

  // Fail for too large chunks:
  if (Container.Size > MaxSize) return false;
  // No zero-sized containers:
  if (Container.Size == 0) return false;

  DataSize = Container.Size;
  PrevDataSize = DataSize;

  // Find division point for bitmap. Consider 17-byte container f.e.
  while (true)
  {
    BitmapSize = (DataSize + BitsInByte - 1) / BitsInByte;
    DataSize = Container.Size - BitmapSize;
    if (DataSize == 0) return false;
    if (DataSize == PrevDataSize) break;
    PrevDataSize = DataSize;
  }

  Data.Addr = Container.Addr;
  Data.Size = DataSize;

  Bitmap.Addr = Data.Addr + Data.Size;
  Bitmap.Size = BitmapSize;

  return true;
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

  // Zero size is discouraged
  if (Size == 0) return false;

  // Return if we have no idea where to insert
  if (!GetInsertIndex(&InsertIndex, Size)) return false;

  // So far <MemSeg.Addr> is just zero-based index
  MemSeg->Addr = InsertIndex;
  MemSeg->Size = Size;

  // Set all bits in bitmap for span
  MarkByteRange(*MemSeg);

  // Now <MemSeg.Addr> is real address
  MemSeg->Addr += Data.Addr;

  // Zero data (design requirement)
  me_WorkmemTools::ZeroMem(*MemSeg);

  LastSegSize = MemSeg->Size;

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
  /*
    We're marking span as free in bitmap.
  */

  // Zero size is discouraged
  if (MemSeg->Size == 0) return false;

  LastSegSize = MemSeg->Size;

  // Segment is not in our memory?
  if (!IsOurs(*MemSeg)) return false;

  // Zero data for security (optional)
  me_WorkmemTools::ZeroMem(*MemSeg);

  // Now <MemSeg.Addr> is zero-based index
  MemSeg->Addr -= Data.Addr;

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
  TUint_2 Limit = Data.Size;
  TUint_2 BestIndex = TUint_2_Max;
  TUint_2 BestDelta = TUint_2_Max;
  TUint_2 BestSpanLen = TUint_2_Max;

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
  TUint_2 Limit = Data.Size;
  TUint_2 Cursor = StartIdx;

  for (Cursor = StartIdx; Cursor < Limit; ++Cursor)
    if (GetBit(Cursor)) break;

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
  return me_AddrsegTools::IsInside(MemSeg, Data);
}

/*
  Return bit value in bitmap's data
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

  ByteAddress = Bitmap.Addr + (BitIndex / BitsInByte);
  ByteValue = me_WorkMemory::Freetown::GetByteFrom(ByteAddress);
  BitOffset = BitIndex % BitsInByte;

  return me_Bits::Freetown::GetBit(&ByteValue, BitOffset);
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

  ByteAddress = Bitmap.Addr + (BitIndex / BitsInByte);
  ByteValue = me_WorkMemory::Freetown::GetByteFrom(ByteAddress);
  BitOffset = BitIndex % BitsInByte;
  me_Bits::SetBitTo(&ByteValue, BitOffset, BitValue);
  me_WorkMemory::Freetown::SetByteAt(ByteAddress, ByteValue);
}

// ) TMemoryAllocator

/*
  2024 # # # #
  2025 # #
  2026-03-17
*/
