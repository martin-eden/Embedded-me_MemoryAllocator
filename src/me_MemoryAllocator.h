// [me_Heap] Dynamic memory management

/*
  Author: Martin Eden
  Last mod.: 2026-03-17
*/

/*
  Experimental module
*/

#pragma once

#include <me_BaseTypes.h>
#include <me_Bits.h>

namespace me_MemoryAllocator
{
  class TMemoryAllocator
  {
    public:
      TBool Init(TAddressSegment);

      TBool Reserve(TAddressSegment *, TUint_2);
      TBool Release(TAddressSegment *);

    protected:
      TAddressSegment Data;
      TAddressSegment Bitmap;
      TUint_2 LastSegSize = 0;

      // Get index of empty span in bitmap where we will allocate
      TBool GetInsertIndex(TUint_2 *, TUint_2);

      // Find nearest busy byte
      TUint_2 GetNextBusyIndex(TUint_2);

      // Set bitmap bits for segment to given value
      void SetByteRange(TAddressSegment, me_Bits::TBitValue);

      // Set bitmap range bits
      void MarkByteRange(TAddressSegment);

      // Clear bitmap range bits
      void ClearByteRange(TAddressSegment);

      // [Sanity] segment in our managed range?
      TBool IsOurs(TAddressSegment);

      // Return bit value in segment's data
      me_Bits::TBitValue GetBit(TUint_2);

      // Set bit to given value in segment's data
      void SetBit(TUint_2, me_Bits::TBitValue);
  };
}

/*
  2024 # #
  2025 #
  2026-03-03
  2026-03-17
*/
