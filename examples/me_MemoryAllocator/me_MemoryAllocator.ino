// [me_MemoryAllocator] Simple test

/*
  Author: Martin Eden
  Last mod.: 2026-03-04
*/

#include <me_MemoryAllocator.h>

#include <me_BaseTypes.h>
#include <me_Console.h>

void Reserve_Wrapper(
  TAddressSegment * MemSeg,
  TUint_2 Size
)
{
  TBool IsOk;

  IsOk = HeapMem.Reserve(MemSeg, Size);

  Console.Write("HeapMem.Reserve");
  Console.Write("(");
  Console.Print(MemSeg->Addr);
  Console.Print(MemSeg->Size);
  Console.Write(")");
  Console.Print(Size);
  Console.Write("->");
  Console.Print(IsOk);
  Console.EndLine();
}

void Release_Wrapper(
  TAddressSegment * MemSeg
)
{
  TBool IsOk;

  Console.Write("HeapMem.Release");
  Console.Write("(");
  Console.Print(MemSeg->Addr);
  Console.Print(MemSeg->Size);
  Console.Write(")");
  Console.Write("->");

  IsOk = HeapMem.Release(MemSeg);

  Console.Print(IsOk);
  Console.EndLine();
}


void RunTest()
{
  /*
    We'll use global object "HeapMem"

    Which is declared in "me_MemoryAllocator.cpp"
  */

  const TUint_2 HeapSize = 400;

  if (!HeapMem.Init(HeapSize))
  {
    Console.Print("Failed to allocate heap memory.");

    return;
  }

  /*
    This test is not doing much at this time. Just allocates/frees
    memory blocks to make holes.

    Real test is add debug printing in memory allocation functions
    in [me_WorkmemTools] and run real code. Copy messages, convert
    them to bitmap. Observe.
  */

  TAddressSegment Seg_1;
  TAddressSegment Seg_2;
  TAddressSegment Seg_3;

  Reserve_Wrapper(&Seg_1, 100);
  Reserve_Wrapper(&Seg_2, 10);
  Reserve_Wrapper(&Seg_3, 60);

  Release_Wrapper(&Seg_2);

  Reserve_Wrapper(&Seg_2, 30);

  Release_Wrapper(&Seg_3);

  Release_Wrapper(&Seg_2);

  Reserve_Wrapper(&Seg_2, 60);

  Release_Wrapper(&Seg_1);

  Release_Wrapper(&Seg_2);
}

void setup()
{
  Console.Init();

  Console.Print("( [me_MemoryAllocator] test");
  Console.Indent();
  RunTest();
  Console.Unindent();
  Console.Print(") Done");
}

void loop()
{
}

/*
  2024 # # # #
  2026-03-04
*/
