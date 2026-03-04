// [me_Heap] Dynamic memory management

/*
  Author: Martin Eden
  Last mod.: 2026-03-03
*/

/*
  Experimental module
*/

#pragma once

#include <me_BaseTypes.h>
#include <me_Bits.h>

#include <me_WorkmemTools.h>

namespace me_MemoryAllocator
{
  class TMemoryAllocator
  {
    public:
      ~TMemoryAllocator();

      TBool Init(TUint_2);
      TBool IsReady();

      TBool Reserve(TAddressSegment *, TUint_2);
      TBool Release(TAddressSegment *);

    protected:
      me_WorkmemTools::TManagedMemory Data;
      me_WorkmemTools::TManagedMemory Bitmap;
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

    private:
      TBool IsReadyFlag = false;
  };
}

/*
  Global class instance

  Used to switch [TManagedMemory] allocators to our allocator
  when we are ready.

  We are ready when

    HeapMem.GetIsReady() == true :

      That means that someone completed HeapMem.Init(<Size>) :

        That means that [TManagedMemory] allocators reserved
        memory for our data and bitmap via stock malloc().

  C developers love to hide globals under "__" prefix. Like
  "__flp" and three more with it. We're instead declaring it
  openly. We're assuming you know framework you working with.

  If you named variable "HeapMem" and then something went wrong,
  you're here to read this comment. Now you know.
*/
extern me_MemoryAllocator::TMemoryAllocator HeapMem;

/*
  2024 # #
  2025 #
  2026-03-03
*/
