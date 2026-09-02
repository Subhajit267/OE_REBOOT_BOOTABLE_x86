/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-30
Date Last Modified: 2026-09-01
Module: Kernel
File: paging.h
About: Identity-mapped (virtual == physical) paging, covering detected
       RAM up to a static cap (see paging.c). Ring-0-only, single
       address space -- no per-process page directories until there's a
       process to give one to.
Revisions:
- 2026-08-30  Initial creation (Phase 3: PMM + paging)
- 2026-09-01  Added paging_get_identity_limit() so callers walking
              firmware-supplied physical pointers (hal/src/acpi.c's ACPI
              table chain) can bounds-check before dereferencing instead
              of assuming every physical address is mapped -- fixes a
              real page fault on VMs with RAM above the identity-map cap.
------------------------------------------------------------
*/

#ifndef KERNEL_PAGING_H
#define KERNEL_PAGING_H

/* Builds an identity-mapped page directory (virtual == physical) covering
   detected RAM up to a static cap, loads it into CR3, and flips CR0.PG.
   Call after pmm_init() -- it reads pmm_get_highest_address() to size the
   map. Ring-0-only, single address space (per the agreed v1 scope: no
   per-process page directories until there's a process to give one to). */
void paging_init(void);

/* Highest physical address (exclusive) actually covered by the identity map
   paging_init() built -- min(detected RAM, the static cap paging.c's own
   comment documents). Anything at or above this is unmapped and will
   page-fault if dereferenced; callers that walk firmware-supplied physical
   pointers (e.g. hal/src/acpi.c's ACPI table chain, which can legitimately
   point anywhere in RAM) must check against this before dereferencing
   instead of assuming every physical address is safe to read. */
unsigned long long paging_get_identity_limit(void);

#endif
