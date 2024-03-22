/** @file
  X64 implementation for address checks.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#include "DebugAgent.h"
#include "VirtualMemory.h"

#define ADDRESS_BITS  0x0000000FFFFFF000ull
//             5         4         3         2         1
//      7654321098765432109876543210987654321098765432109876543210
//      |       |       |        |       |       |       |       |
//                000000000000000000000000000000000000000000000000
//                                           111111111
//                                  111111111
//                         111111111
//                111111111
//       111111111
//
//
#define Pml5Index(a)   ((UINT64) ((a & 0x01FF000000000000ull)) >> 47) // 12+9+9+9+9
#define Pml4Index(a)   ((UINT64) ((a & 0x0000FF8000000000ull)) >> 39) // 12+9+9+9
#define PdpteIndex(a)  ((UINT64) ((a & 0x0000007FC0000000ull)) >> 30) // 12+9+9
#define PdeIndex(a)    ((UINT64) ((a & 0x000000003FE00000ull)) >> 21) // 12+9
#define PteIndex(a)    ((UINT64) ((a & 0x00000000001FF000ull)) >> 12) // 12

typedef enum {
  PAGE_IS_NOT_VALID,
  PAGE_IS_READ_ONLY,
  PAGE_IS_READ_WRITE
} PAGE_IS;

/**
  Walks the page table to find the virtual address entry and returns is it is
  readable, writable, or invalid.

  @param[in]  Address     The virtual address to check.

  @retval     PAGE_IS_NOT_VALID     The virtual address is not valid.
  @retval     PAGE_IS_READ_ONLY     The virtual address is read only.
  @retval     PAGE_IS_READ_WRITE    The virtual address is read/write.
**/
STATIC
PAGE_IS
GetPageIs (
  IN UINT64  Address
  )
{
  PAGE_MAP_AND_DIRECTORY_POINTER  *Pml5;
  PAGE_MAP_AND_DIRECTORY_POINTER  *Pml4;
  PAGE_TABLE_1G_ENTRY             *Pte1G;
  PAGE_TABLE_ENTRY                *Pte2M;
  PAGE_TABLE_4K_ENTRY             *Pte4K;
  PAGE_IS                         PageIs;
  UINT64                          Cr3;
  IA32_CR4                        Cr4;

  if (Address == 0) {
    return PAGE_IS_NOT_VALID;
  }

  if ((Address >= 0x83000000) && (Address <= 0x87c00000)) {
    return PAGE_IS_NOT_VALID;
  }

  Cr4.UintN = AsmReadCr4 ();
  Cr3       = AsmReadCr3 () & ADDRESS_BITS;
  if (Cr3 == 0) {
    return PAGE_IS_NOT_VALID;
  }

  if (Cr4.Bits.LA57) {
    // 5 level paging
    Pml5 = (PAGE_MAP_AND_DIRECTORY_POINTER *)(Cr3 + Pml5Index (Address));

    if (!Pml5->Bits.Present) {
      return PAGE_IS_NOT_VALID;
    }

    Pml4 = (PAGE_MAP_AND_DIRECTORY_POINTER *)((Pml5->Uint64 & ADDRESS_BITS) + Pml4Index (Address));
  } else {
    Pml4 = (PAGE_MAP_AND_DIRECTORY_POINTER *)(Cr3 + Pml4Index (Address));
  }

  // While a page table may be at address 0, it is unlikely as page 0 is usually reserved as
  // an invalid page to prevent a NULL pointer access.
  if (Pml4 == NULL) {
    return PAGE_IS_NOT_VALID;
  }

  if (!Pml4->Bits.Present) {
    return PAGE_IS_NOT_VALID;
  }

  Pte1G = (PAGE_TABLE_1G_ENTRY *)(Pml4->Uint64 & ADDRESS_BITS) + PdpteIndex (Address);

  if (Pte1G == NULL) {
    return PAGE_IS_NOT_VALID;
  }

  if (!Pte1G->Bits.Present) {
    return PAGE_IS_NOT_VALID;
  }

  if (Pte1G->Bits.MustBe1) {
    // 1GB Page table entry
    PageIs = (Pte1G->Bits.ReadWrite) ? PAGE_IS_READ_WRITE : PAGE_IS_READ_ONLY;
    return PageIs;
  }

  Pte2M = (PAGE_TABLE_ENTRY *)(Pte1G->Uint64 & ADDRESS_BITS) + PdeIndex (Address);
  if (Pte2M == NULL) {
    return PAGE_IS_NOT_VALID;
  }

  if (!Pte2M->Bits.Present) {
    return PAGE_IS_NOT_VALID;
  }

  if (Pte2M->Bits.MustBe1) {
    PageIs = Pte2M->Bits.ReadWrite ? PAGE_IS_READ_WRITE : PAGE_IS_READ_ONLY;
    return PageIs;
  }

  Pte4K = (PAGE_TABLE_4K_ENTRY *)(Pte2M->Uint64 & ADDRESS_BITS) + PteIndex (Address);
  if (Pte4K == NULL) {
    return PAGE_IS_NOT_VALID;
  }

  if (!Pte4K->Bits.Present) {
    return PAGE_IS_NOT_VALID;
  }

  PageIs = (Pte4K->Bits.ReadWrite) ? PAGE_IS_READ_WRITE : PAGE_IS_READ_ONLY;
  return PageIs;
}

/**
  Checks if a given virtual address is readable.

  @param[in]  Address     The virtual address to check.

  @retval     TRUE        The address is readable.
  @retval     FALSE       The address is not readable.
**/
BOOLEAN
IsPageReadable (
  IN UINT64  Address
  )
{
  PAGE_IS  PageIs;

  PageIs = GetPageIs (Address);
  return ((PageIs == PAGE_IS_READ_ONLY) || (PageIs == PAGE_IS_READ_WRITE));
}

/**
  Checks if a given virtual address is writable.

  @param[in]  Address     The virtual address to check.

  @retval     TRUE        The address is writable.
  @retval     FALSE       The address is not writable.
**/
BOOLEAN
IsPageWritable (
  IN UINT64  Address
  )
{
  PAGE_IS  PageIs;

  PageIs = GetPageIs (Address);
  return (PageIs == PAGE_IS_READ_WRITE);
}
