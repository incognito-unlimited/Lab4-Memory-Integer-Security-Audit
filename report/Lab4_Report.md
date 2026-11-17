# Lab 4 — Unit I Practical Assignment: Memory and Integer Security Audit

| Field | Value |
|---|---|
| Submitted by | Aditey Mehra |
| Roll number | 2301730266 |
| University | K.R. Mangalam University |
| Programme | B.Tech Computer Science and Engineering |
| Specialisation | Artificial Intelligence and Machine Learning |
| Section | D |
| Semester | 7 |
| Subject | Secure Coding and Vulnerabilities |

## 1. Aim

To audit the supplied Student C module for memory-management and integer-security defects, remediate the confirmed issues, validate the corrected implementation with compiler hardening flags, Valgrind Memcheck, AddressSanitizer, UndefinedBehaviorSanitizer and cppcheck, and publish the complete evidence.

## 2. Objectives

- Identify allocation, bounds, deallocation, ownership and lifetime defects.
- Identify signed overflow, negative-to-unsigned conversion and allocation-size overflow.
- Record every confirmed defect with an accurate source line and security impact.
- Refactor with checked allocations, bounded strings, safe size calculations and checked arithmetic.
- Compile both versions with equivalent warning and hardening flags.
- Run Valgrind and ASan/UBSan on the corrected module and preserve complete output.
- Compare diagnostics, execute additional test cases and publish the work to GitHub.

## 3. Introduction

The supplied module dynamically allocates a `Student` object, a name string and an array of marks. Its functions create, total, update, resize, display and destroy a student record. The original implementation contains defects in allocation handling, string storage, array bounds, arithmetic, resizing, deallocation and object lifetime. This report is based only on the exact 76-line faculty source preserved as `original/student.c` (SHA-256 `3de3ffedbc41e951332bcaa36227bd15b97fd5fca1dff5250b2b1b8ac1aff5e4`).

## 4. Original Source Code

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define MAX_NAME 32
 
typedef struct {
    char *name;
    int *marks;
    size_t count;
} Student;
 
Student *create_student(const char *name, int count)
{
    Student *s = malloc(sizeof(Student));
    if (strlen(name) > MAX_NAME)
        printf("Warning: name is too long\n");
    s->name = malloc(strlen(name));
    strcpy(s->name, name);
    s->count = count;
    s->marks = malloc(count * sizeof(int));
    return s;
}
 
int calculate_total(Student *s)
{
    int total = 0;
    for (int i = 0; i <= s->count; i++)
        total += s->marks[i];
    return total;
}
 
void add_bonus(Student *s, int bonus)
{
    for (int i = 0; i < s->count; i++)
        s->marks[i] += bonus;
}
 
void resize_marks(Student *s, int new_count)
{
    s->marks = realloc(s->marks, new_count * sizeof(int));
    s->count = new_count;
}
 
void display_student(Student *s)
{
    printf("Student: %s\n", s->name);
    for (int i = 0; i < s->count; i++)
        printf("Mark[%d] = %d\n", i, s->marks[i]);
}
 
void destroy_student(Student *s)
{
    free(s->name);
    free(s->marks);
    free(s->name);
    free(s);
}
 
int main(void)
{
    Student *student = create_student("Alice", 5);
    student->marks[0] = 80;
    student->marks[1] = 75;
    student->marks[2] = 90;
    student->marks[3] = 85;
    student->marks[4] = 95;
    display_student(student);
    printf("Total = %d\n", calculate_total(student));
    add_bonus(student, 5);
    resize_marks(student, 10);
    display_student(student);
    destroy_student(student);
    printf("First mark = %d\n", student->marks[0]); /* UAF */
    return 0;
}
```

## 5. Defect Identification and Classification

The complete 19-row table is in `report/defect_table.md`. Summary:

| ID | Function | Lines | Category | Severity |
|---|---|---|---|---|
| D-01 | `create_student` | 15, 18 | Unchecked `malloc` / null dereference | High |
| D-02 | `create_student` | 16 | NULL input not validated | High |
| D-03 | `create_student` | 18 | Missing terminator / undersized allocation | High |
| D-04 | `create_student` | 19 | Heap buffer overflow (`strcpy`) | High |
| D-05 | `create_student` | 16–17 | Warning-only length validation | Medium |
| D-06 | `create_student` | 21 | Unchecked marks allocation | High |
| D-07 | `create_student` | 20, 21 | Negative-to-unsigned conversion | High |
| D-08 | `create_student`, `resize_marks` | 21, 41 | Allocation-size multiplication overflow | High |
| D-09 | `calculate_total` | 28–29 | Off-by-one / out-of-bounds read | High |
| D-10 | `calculate_total` | 29 | Signed integer overflow | High |
| D-11 | `add_bonus` | 36 | Signed overflow / partial update | High |
| D-12 | `resize_marks` | 41 | Unsafe `realloc` assignment | High |
| D-13 | `resize_marks` | 39, 41–42 | Invalid resize count | High |
| D-14 | `resize_marks` | 41–42 | Uninitialised grown elements | Medium |
| D-15 | all API functions | 25, 33, 39, 45, 52 | Missing argument validation | Medium |
| D-16 | `destroy_student` | 54, 56 | Double free | High |
| D-17 | `main` | 73–74 | Use-after-free | High |
| D-18 | `main` | 62–63 | Unchecked constructor result | High |
| D-19 | total/bonus/display | 28, 35, 48 | Unsafe signed index type | Medium |

No wrong-pairing `free`, overlapping `memcpy`, normal-path leak or explicit narrowing cast was claimed without evidence.

## 6. Security Impact Summary

- **Availability:** allocation failures, invalid accesses and sanitizer-detected defects crash the process.
- **Integrity:** heap overflow, out-of-bounds writes and signed overflow corrupt program state and totals.
- **Confidentiality:** use-after-free and out-of-bounds reads can expose stale or adjacent heap memory.
- **Memory safety:** double free and a lost `realloc` pointer corrupt allocator state or leak resources.
- **Integer safety:** signed-to-unsigned conversion and unchecked multiplication produce undersized allocations and invalid counts.

The original under Valgrind produced **27 errors from 10 contexts**, confirming the invalid write, out-of-bounds read, uninitialised-value use, invalid free and use-after-free.

## 7. Remediation Strategy

1. Validate pointers and counts at every entry point; use `size_t` and reject zero.
2. Scan names up to `MAX_NAME`, reject length 0 or over-limit, allocate `length + 1`, copy with `memcpy()`.
3. Check every allocation and unwind partial allocations before returning NULL.
4. Check `count <= SIZE_MAX / sizeof(int)` before multiplication.
5. Accumulate in `int64_t` with before-addition range checks; return a status.
6. Prevalidate all bonus results against `INT_MIN`/`INT_MAX` before modifying any element.
7. Use `realloc()` through a temporary pointer and zero the newly grown region.
8. Use `size_t` indices and `%zu` formats.
9. Free each allocation once; take `Student **` in `destroy_student` and null the caller pointer.
10. Read the first mark before destruction; never access the object afterwards.

## 8. Remediated Source Code

The complete implementation is `remediated/student.c`. Its key functions are:

```c
static int checked_array_bytes(size_t count, size_t element_size, size_t *bytes)
{
    if ((bytes == NULL) || (element_size == 0u) ||
        (count > SIZE_MAX / element_size)) {
        return 0;
    }
    *bytes = count * element_size;
    return 1;
}

Student *create_student(const char *name, size_t count)
{
    Student *student = NULL;
    size_t name_length = 0u;
    size_t marks_bytes = 0u;

    if (!bounded_name_length(name, &name_length) || (count == 0u) ||
        !checked_array_bytes(count, sizeof(int), &marks_bytes)) {
        return NULL;
    }

    student = calloc(1u, sizeof(*student));
    if (student == NULL) {
        return NULL;
    }

    student->name = malloc(name_length + 1u);
    if (student->name == NULL) {
        free(student);
        return NULL;
    }
    memcpy(student->name, name, name_length + 1u);

    student->marks = calloc(1u, marks_bytes);
    if (student->marks == NULL) {
        free(student->name);
        free(student);
        return NULL;
    }
    student->count = count;
    return student;
}

int calculate_total(const Student *student, int64_t *total_out)
{
    int64_t total = 0;

    if ((student == NULL) || (total_out == NULL) ||
        ((student->count > 0u) && (student->marks == NULL))) {
        return 0;
    }

    for (size_t index = 0u; index < student->count; index++) {
        const int64_t value = (int64_t)student->marks[index];

        if ((value > 0) && (total > INT64_MAX - value)) {
            return 0;
        }
        if ((value < 0) && (total < INT64_MIN - value)) {
            return 0;
        }
        total += value;
    }

    *total_out = total;
    return 1;
}

int add_bonus(Student *student, int bonus)
{
    const int64_t bonus_value = (int64_t)bonus;

    if ((student == NULL) ||
        ((student->count > 0u) && (student->marks == NULL))) {
        return 0;
    }

    /* Prevalidate every result so a later overflow cannot partially update. */
    for (size_t index = 0u; index < student->count; index++) {
        const int64_t result = (int64_t)student->marks[index] + bonus_value;

        if ((result < (int64_t)INT_MIN) || (result > (int64_t)INT_MAX)) {
            return 0;
        }
    }

    for (size_t index = 0u; index < student->count; index++) {
        student->marks[index] = (int)((int64_t)student->marks[index] + bonus_value);
    }
    return 1;
}

int resize_marks(Student *student, size_t new_count)
{
    size_t new_bytes = 0u;
    int *resized = NULL;

    if ((student == NULL) || (new_count == 0u) ||
        !checked_array_bytes(new_count, sizeof(*student->marks), &new_bytes)) {
        return 0;
    }

    resized = realloc(student->marks, new_bytes);
    if (resized == NULL) {
        return 0;
    }

    if (new_count > student->count) {
        memset(resized + student->count, 0,
               (new_count - student->count) * sizeof(*resized));
    }

    student->marks = resized;
    student->count = new_count;
    return 1;
}

void destroy_student(Student **student)
{
    Student *victim = NULL;

    if ((student == NULL) || (*student == NULL)) {
        return;
    }

    victim = *student;
    free(victim->name);
    free(victim->marks);
    free(victim);
    *student = NULL;
}
```

**Documented API/behaviour changes:** count parameters are `size_t`, destructive operations return status values, and `destroy_student` takes `Student **`. The Alice scenario still displays the five initial marks, the total 425, the updated marks and zeroes in the grown slots, and the first mark is printed before destruction.

## 9. Compilation with Hardening Flags

Environment: gcc 15.2.0, clang 21.1.8, Ubuntu 26.04.1 LTS on Linux 6.18.58.1-microsoft-standard-WSL2. Both versions were compiled with:

```
-std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
-Wshadow -Wformat=2 -Wstrict-prototypes -fstack-protector-strong
-D_FORTIFY_SOURCE=2 -O2 -fPIE -pie -Wl,-z,relro,-z,now
```

GNU ELF linker hardening is supported here. On macOS, `-Wl,-z,relro,-z,now` would be omitted in favour of platform-supported options. Both builds exited 0. Complete logs: `evidence/compiler_original.txt`, `evidence/compiler_fixed.txt`.

## 10. Valgrind and ASan/UBSan Verification

Valgrind 3.26.0 on the hardened remediated binary:

```
valgrind --tool=memcheck --track-origins=yes --leak-check=full \
  --show-leak-kinds=all --errors-for-leak-kinds=all ./build/student_fixed
```

Result: `in use at exit: 0 bytes in 0 blocks`; `5 allocs, 5 frees, 4,186 bytes allocated`; `All heap blocks were freed -- no leaks are possible`; `ERROR SUMMARY: 0 errors from 0 contexts`; exit status 0. No invalid reads/writes, invalid frees, double frees, use-after-free or uninitialised-value errors occurred.

ASan/UBSan with clang 21.1.8:

```
clang -std=c11 -g -O1 -Wall -Wextra -fsanitize=address,undefined \
  -fno-omit-frame-pointer remediated/student.c -o build/student_asan_ubsan
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ./build/student_asan_ubsan
```

Build exit 0, run exit 0, and no sanitizer diagnostics. Leak detection is supported on this platform. Complete logs: `evidence/valgrind_fixed.txt`, `evidence/asan_ubsan_fixed.txt`.

## 11. Before-and-After Compiler Warning Comparison

| Category | Original | Remediated |
|---|---:|---:|
| Format and type warnings | 0 | 0 |
| Signed/unsigned conversion (`-Wsign-conversion`) | 4 | 0 |
| Bounds / array diagnostics | 0 | 0 |
| Allocation / nullability | 0 | 0 |
| Other (`-Wsign-compare`) | 3 | 0 |
| **Total warnings** | **7** | **0** |

All seven original warnings were removed: sign-conversion at lines 20, 21, 41 and 42, and signed/unsigned comparisons at lines 28, 35 and 48. None remain and none were introduced. GCC did not warn about the off-by-one, unsafe string copy, unsafe `realloc`, double free or use-after-free; those were confirmed by source review, cppcheck and Valgrind.

## 12. Defect Remediation Summary

| Defect area | Verification |
|---|---|
| Unchecked allocation / NULL dereference | Failure-injection tests, cppcheck, Valgrind |
| String overflow and missing terminator | Boundary tests and Valgrind |
| Off-by-one total loop | Correct loop plus Valgrind original comparison |
| Signed arithmetic overflow | Boundary tests and UBSan |
| Unsafe realloc and lost pointer | Injected realloc failure test |
| Uninitialised grown slots | Valgrind and resize test |
| Double free | Single-free destructor, Valgrind |
| Use-after-free | Read-before-destroy, null caller pointer, Valgrind |

Nineteen controlled test checks passed with exit 0 (`evidence/test_results.txt`). cppcheck fell from 12 findings to 6; the six remaining `staticFunction` advisories are stylistic consequences of the deliberately non-static testable API.

## 13. GitHub Repository

Repository URL: https://github.com/incognito-unlimited/Lab4-Memory-Integer-Security-Audit

## 14. Result

The exact faculty module was preserved and 19 defects were confirmed with accurate line numbers. The remediated module compiled without warnings under GCC and clang with hardening flags. Valgrind reported zero errors and no leaks; ASan/UBSan reported no diagnostics. GCC warnings fell from 7 to 0, and cppcheck findings fell from 12 to 6, with all memory- and integer-related cppcheck diagnostics resolved. All 19 additional checks passed. Ten genuine screenshots and all raw evidence were published.

## 15. Conclusion

The audit shows that memory and integer defects interact to create serious reliability and security problems. Safe allocation, bounded strings, checked size arithmetic, range-checked arithmetic, correct array bounds, safe reallocation, single ownership and clear object lifetime are all necessary in secure C. Compiler warnings, static analysis and dynamic tools provide complementary evidence; none alone proves the absence of every defect, so conclusions are limited to the tested executions and the reviewed source.
