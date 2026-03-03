# What

(2024-10)

Experimental. Memory allocator. Arduino library.

Currently it's "best fit" strategy with attaching allocated memory
block to the right of free segment if size of block is less than
size of previously allocated block.

Implementation uses bitmap to mark busy bytes. Complexity is linear of
memory size. Implementation can be made faster. But the point of this
module is explore strategies and readable code.


## Sample output

```
[me_Heap] Start.
[Heap] Reserve ( Addr 00557 Size 00100 )
[Heap] Reserve ( Addr 01347 Size 00010 )
[Heap] Reserve ( Addr 00657 Size 00060 )
[Heap] Release ( Addr 01347 Size 00010 )
[Heap] Reserve ( Addr 00717 Size 00030 )
[Heap] Release ( Addr 00657 Size 00060 )
[Heap] Release ( Addr 00717 Size 00030 )
[Heap] Reserve ( Addr 00657 Size 00060 )
[Heap] Release ( Addr 00557 Size 00100 )
[Heap] Release ( Addr 00657 Size 00060 )
[me_Heap] Done.
```

## Requirements

  * arduino-cli
  * bash


## Install/remove

Easy way is to clone [GetLibs][GetLibs] repo and run it's code.


# Compile

Zero-warnings compilation:

```bash
arduino-cli compile --fqbn arduino:avr:uno --quiet --warnings all . --build-property compiler.cpp.extra_flags="-std=c++1z"
```

# Code

* [Example][Example]
* [Interface][Interface]
* [Implementation][Implementation]


# See also

* [My other embedded C++ libraries][Embedded]
* [My other repositories][Repos]

[Example]: examples/me_Heap/me_Heap.ino
[Interface]: src/me_Heap.h
[Implementation]: src/me_Heap.cpp

[GetLibs]: https://github.com/martin-eden/Embedded-Framework-GetLibs

[Embedded]: https://github.com/martin-eden/Embedded_Crafts/tree/master/Parts
[Repos]: https://github.com/martin-eden/contents
