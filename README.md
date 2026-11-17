# Lab 4 — Memory and Integer Security Audit

**Unit I Practical Assignment**  
**Subject:** Secure Coding and Vulnerabilities  
**Student:** Aditey Mehra  
**Roll number:** 2301730266  
**University:** K.R. Mangalam University  
**Programme:** B.Tech Computer Science and Engineering (AI & ML)  
**Section:** D — Semester 7

## Aim

Audit the exact faculty-provided Student C module for memory-management and integer-security defects, remediate every confirmed finding, compile both versions with hardening flags, verify the corrected program with Valgrind Memcheck and ASan/UBSan, compare compiler and cppcheck diagnostics, collect genuine evidence, populate the editable DOCX report, and publish the project to GitHub.

## Original module

`original/student.c` is the unchanged 76-line faculty module extracted verbatim from the supplied report template. It defines a `Student` structure (`name`, `marks`, `count`) and the functions `create_student`, `calculate_total`, `add_bonus`, `resize_marks`, `display_student`, `destroy_student` and `main`.

SHA-256: `3de3ffedbc41e951332bcaa36227bd15b97fd5fca1dff5250b2b1b8ac1aff5e4`

## Defect classification

Nineteen defects were confirmed with exact line numbers in `report/defect_table.md`: 15 High and 4 Medium severity. They include unchecked allocations, missing NUL terminator, unbounded `strcpy()`, off-by-one read, signed overflow in total and bonus calculation, allocation-size multiplication overflow, unsafe `realloc()`, uninitialised grown elements, double free, use-after-free, missing validation and unsafe signed index types.

The original under Valgrind produced **27 errors from 10 contexts**, including an invalid heap write, an out-of-bounds read, use of uninitialised values, an invalid free and use-after-free. cppcheck reported 12 findings including `doubleFree` and four `nullPointerOutOfMemory` diagnostics.

## Remediation summary

- Validated all pointers and counts; `size_t` public counts reject zero and overflow before multiplication.
- Bounded name scan up to `MAX_NAME`; allocated `length + 1` and copied the terminator with `memcpy()`.
- Checked every allocation and unwound partial allocations on failure.
- Accumulated totals in `int64_t` with `INT64_MAX`/`INT64_MIN` guards and returned a status.
- Prevalidated all bonus results against `INT_MIN`/`INT_MAX` so failures cannot partially update the array.
- Used a temporary pointer for `realloc()` and zero-initialised newly grown elements.
- Used `size_t` indices and `%zu` formats; validated object and member pointers in every function.
- Freed each resource exactly once, changed `destroy_student` to `Student **`, nulled the caller pointer, and read the first mark before destruction.

Documented API changes: `create_student`, `resize_marks` and `destroy_student` have safer signatures; `calculate_total`, `add_bonus` and `resize_marks` now return status values. The original Alice scenario and its displayed output are preserved.

## Compiler and hardening flags

```
gcc --version        -> gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
clang --version      -> Ubuntu clang version 21.1.8 (6ubuntu1)

-std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
-Wshadow -Wformat=2 -Wstrict-prototypes
-fstack-protector-strong -D_FORTIFY_SOURCE=2 -O2 -fPIE -pie
-Wl,-z,relro,-z,now
```

The environment is Linux/ELF (Ubuntu 26.04.1 LTS, WSL2), so the GNU linker hardening flags are supported. On macOS they should be omitted and the platform's supported hardening options used instead.

| Category | Original | Remediated |
|---|---:|---:|
| Format and type warnings | 0 | 0 |
| Signed/unsigned conversion | 4 | 0 |
| Bounds / array diagnostics | 0 | 0 |
| Allocation / nullability | 0 | 0 |
| Other (`-Wsign-compare`) | 3 | 0 |
| **Total** | **7** | **0** |

Details: `evidence/compiler_comparison.txt`, `evidence/compiler_original.txt`, `evidence/compiler_fixed.txt`.

## Valgrind Memcheck

```
valgrind --tool=memcheck --track-origins=yes --leak-check=full \
  --show-leak-kinds=all --errors-for-leak-kinds=all ./build/student_fixed
```

Result: exit 0, `HEAP SUMMARY: in use at exit: 0 bytes in 0 blocks`, `total heap usage: 5 allocs, 5 frees, 4,186 bytes allocated`, `All heap blocks were freed -- no leaks are possible`, `ERROR SUMMARY: 0 errors from 0 contexts`. No invalid reads, writes, frees, double frees, use-after-free or uninitialised-value errors occurred in the tested execution. Complete log: `evidence/valgrind_fixed.txt`. Supporting evidence for the original: `evidence/valgrind_original.txt`.

## ASan / UBSan

```
clang -std=c11 -g -O1 -Wall -Wextra -fsanitize=address,undefined \
  -fno-omit-frame-pointer remediated/student.c -o build/student_asan_ubsan

ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ./build/student_asan_ubsan
```

Build exit 0, run exit 0, no AddressSanitizer or UndefinedBehaviorSanitizer diagnostics. Leak detection is supported on this Linux host. Complete log: `evidence/asan_ubsan_fixed.txt`.

## cppcheck

```
cppcheck --enable=all --inconclusive --std=c11 \
  --suppress=missingIncludeSystem --template=gcc <file> 2>&1
```

Cppcheck 2.19.0 reported 12 findings for the original and 6 for the remediated source. Resolved: `doubleFree` ×1, `nullPointerOutOfMemory` ×4, `constParameterPointer` ×1. Remaining: six `staticFunction` advisories caused by the deliberately non-static testable API; none are memory or integer defects. Cppcheck does not prove the absence of all security defects. Details: `evidence/cppcheck_comparison.txt`.

## Additional testing

Nineteen controlled checks passed with exit 0, covering normal creation, the exact `MAX_NAME` boundary, over-length and NULL names, zero and excessive counts, normal and near-limit totals, in-range and overflowing bonuses, larger/smaller/invalid resizes, injected `malloc`/`calloc`/`realloc` failures, and repeated destruction through a NULL pointer. Allocation failures were injected with linker `--wrap`; no undefined behaviour was used to produce results. Complete output: `evidence/test_results.txt`.

## Repository structure

```
Lab4-Memory-Integer-Security-Audit/
├── original/student.c
├── remediated/student.c
├── tests/test_harness.c
├── report/
│   ├── defect_table.md
│   ├── Lab4_Report.md
│   └── Lab_4_Unit_I_Memory_Integer_Security_Audit_Aditey_Mehra.docx
├── evidence/
│   ├── compiler_original.txt
│   ├── compiler_fixed.txt
│   ├── compiler_comparison.txt
│   ├── compiler_versions.txt
│   ├── compiler_fixed_clang.txt
│   ├── test_harness_build.txt
│   ├── valgrind_fixed.txt
│   ├── valgrind_original.txt
│   ├── asan_ubsan_fixed.txt
│   ├── cppcheck_original.txt
│   ├── cppcheck_fixed.txt
│   ├── cppcheck_comparison.txt
│   ├── test_results.txt
│   └── screenshots/ (01-10 genuine PNG captures)
├── README.md
└── .gitignore
```
