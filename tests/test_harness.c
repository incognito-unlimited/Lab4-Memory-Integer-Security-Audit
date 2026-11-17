#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>
#include <stddef.h>

#define MAX_NAME 32

typedef struct {
    char *name;
    int *marks;
    size_t count;
} Student;

Student *create_student(const char *name, size_t count);
int calculate_total(const Student *student, int64_t *total_out);
int add_bonus(Student *student, int bonus);
int resize_marks(Student *student, size_t new_count);
void display_student(const Student *student);
void destroy_student(Student **student);

/* Linker-wrapped allocator entry points for controlled failure injection. */
extern void *__real_malloc(size_t size);
extern void *__real_calloc(size_t count, size_t size);
extern void *__real_realloc(void *pointer, size_t size);

static int fail_malloc_call = 0;
static int malloc_calls = 0;
static int fail_calloc_call = 0;
static int calloc_calls = 0;
static int fail_realloc_call = 0;
static int realloc_calls = 0;

static void reset_allocator_faults(void)
{
    fail_malloc_call = 0;
    malloc_calls = 0;
    fail_calloc_call = 0;
    calloc_calls = 0;
    fail_realloc_call = 0;
    realloc_calls = 0;
}

void *__wrap_malloc(size_t size)
{
    malloc_calls++;
    if ((fail_malloc_call != 0) && (malloc_calls == fail_malloc_call)) {
        return NULL;
    }
    return __real_malloc(size);
}

void *__wrap_calloc(size_t count, size_t size)
{
    calloc_calls++;
    if ((fail_calloc_call != 0) && (calloc_calls == fail_calloc_call)) {
        return NULL;
    }
    return __real_calloc(count, size);
}

void *__wrap_realloc(void *pointer, size_t size)
{
    realloc_calls++;
    if ((fail_realloc_call != 0) && (realloc_calls == fail_realloc_call)) {
        return NULL;
    }
    return __real_realloc(pointer, size);
}

static int failures = 0;

static void report(int condition, int test_number, const char *description)
{
    printf("TEST %02d: %-58s %s\n", test_number, description,
           condition ? "PASS" : "FAIL");
    if (!condition) {
        failures++;
    }
}

int main(void)
{
    Student *student = NULL;
    int64_t total = 0;
    int before = 0;

    printf("LAB 4 ADDITIONAL TEST CASES - CONTROLLED EXECUTION\n");
    printf("===================================================\n\n");

    /* 1. Normal student creation. */
    reset_allocator_faults();
    student = create_student("Alice", 5u);
    report((student != NULL) && (student->name != NULL) &&
           (student->marks != NULL) && (student->count == 5u) &&
           (strcmp(student->name, "Alice") == 0),
           1, "Normal creation for name 'Alice' and 5 marks");
    destroy_student(&student);

    /* 2. Name exactly at MAX_NAME (32 characters) is accepted. */
    {
        const char boundary_name[MAX_NAME + 1u] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef"; /* exactly 32 characters */
        student = create_student(boundary_name, 2u);
        report((student != NULL) && (strlen(student->name) == (size_t)MAX_NAME) &&
               (strcmp(student->name, boundary_name) == 0),
               2, "Name exactly MAX_NAME characters accepted");
        destroy_student(&student);
    }

    /* 3. Name exceeding MAX_NAME is rejected. */
    student = create_student("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefg", 2u);
    report(student == NULL, 3, "Name exceeding MAX_NAME rejected");
    destroy_student(&student);

    /* 4. NULL name is rejected without an invalid read. */
    student = create_student(NULL, 2u);
    report(student == NULL, 4, "NULL name rejected");
    destroy_student(&student);

    /* 5. Zero marks count is rejected. */
    student = create_student("Alice", 0u);
    report(student == NULL, 5, "Zero marks count rejected");
    destroy_student(&student);

    /* 6. Excessive count (SIZE_MAX) is rejected by the size check. */
    student = create_student("Alice", SIZE_MAX);
    report(student == NULL, 6, "Excessive count rejected before allocation");
    destroy_student(&student);

    /* 7. Normal total calculation, including zero-initialised allocation. */
    student = create_student("Alice", 5u);
    if (student != NULL) {
        static const int marks[5] = {80, 75, 90, 85, 95};
        for (size_t index = 0u; index < 5u; index++) {
            student->marks[index] = marks[index];
        }
        total = -1;
    }
    report((student != NULL) && calculate_total(student, &total) && (total == 425),
           7, "Normal total calculation equals 425");

    /* 8. Total near 32-bit integer limits is computed in int64_t. */
    if (student != NULL) {
        for (size_t index = 0u; index < student->count; index++) {
            student->marks[index] = INT_MAX;
        }
        total = 0;
    }
    report((student != NULL) && calculate_total(student, &total) &&
           (total == (int64_t)INT_MAX * 5),
           8, "Total with five INT_MAX marks uses int64_t accumulation");

    /* 9. Bonus addition that remains within INT range. */
    if (student != NULL) {
        student->marks[0] = 80;
        student->marks[1] = 75;
        student->marks[2] = 90;
        student->marks[3] = 85;
        student->marks[4] = 95;
    }
    before = (student != NULL) ? student->marks[0] : 0;
    report((student != NULL) && add_bonus(student, 5) &&
           (student->marks[0] == before + 5),
           9, "Bonus +5 applied within INT range");

    /* 10. Bonus that would exceed INT_MAX fails with no partial update. */
    if (student != NULL) {
        student->marks[0] = INT_MAX;
        student->marks[1] = INT_MAX - 10;
        student->marks[4] = INT_MAX - 10;
    }
    {
        const int saved_second = (student != NULL) ? student->marks[1] : 0;
        report((student != NULL) && !add_bonus(student, 50) &&
               (student->marks[1] == saved_second) &&
               (student->marks[0] == INT_MAX),
               10, "Overflowing bonus rejected without partial modification");
    }

    /* 11. Resize to a larger array zero-initialises the new elements. */
    if (student != NULL) {
        student->marks[0] = 11;
        student->marks[1] = 22;
    }
    report((student != NULL) && resize_marks(student, 10u) &&
           (student->count == 10u) && (student->marks[0] == 11) &&
           (student->marks[1] == 22) && (student->marks[5] == 0) &&
           (student->marks[9] == 0),
           11, "Resize 5 -> 10 preserves data and zeroes new slots");

    /* 12. Resize to a smaller array preserves existing data. */
    report((student != NULL) && resize_marks(student, 3u) &&
           (student->count == 3u) && (student->marks[0] == 11) &&
           (student->marks[1] == 22),
           12, "Resize 10 -> 3 preserves existing elements");

    /* 13. Invalid resize counts are rejected and state is preserved. */
    if (student != NULL) {
        student->marks[0] = 77;
    }
    {
        int *const saved_pointer = (student != NULL) ? student->marks : NULL;
        const size_t saved_count = (student != NULL) ? student->count : 0u;
        report((student != NULL) && !resize_marks(student, 0u) &&
               !resize_marks(student, SIZE_MAX) &&
               (student->marks == saved_pointer) &&
               (student->count == saved_count) && (student->marks[0] == 77),
               13, "Invalid resize counts rejected; original data preserved");
    }
    destroy_student(&student);

    /* 14. Allocation failures are handled safely with injected failures. */
    reset_allocator_faults();
    fail_malloc_call = 1;
    student = create_student("Alice", 5u);
    report(student == NULL, 14, "First malloc (name) failure returns NULL");
    destroy_student(&student);

    reset_allocator_faults();
    fail_calloc_call = 1;
    student = create_student("Alice", 5u);
    report(student == NULL, 15, "Student calloc failure returns NULL");
    destroy_student(&student);

    reset_allocator_faults();
    fail_calloc_call = 2;
    student = create_student("Alice", 5u);
    report(student == NULL, 16, "Marks calloc failure unwinds partial allocation");
    destroy_student(&student);

    reset_allocator_faults();
    student = create_student("Alice", 5u);
    if (student != NULL) {
        student->marks[0] = 99;
        fail_realloc_call = 1;
    }
    {
        int *const saved_pointer = (student != NULL) ? student->marks : NULL;
        const int result = (student != NULL) ? resize_marks(student, 10u) : 1;
        report((student != NULL) && !result &&
               (student->marks == saved_pointer) &&
               (student->count == 5u) && (student->marks[0] == 99),
               17, "realloc failure preserves original pointer and data");
    }
    reset_allocator_faults();

    /* 15. Repeated destruction through a NULL caller pointer is safe. */
    destroy_student(&student);
    report(student == NULL, 18, "Destroy nulls the caller's pointer");
    destroy_student(&student);
    destroy_student(NULL);
    report(1, 19, "Repeated destroy and NULL caller pointer are safe");

    printf("\nSUMMARY: %d checks executed, %d failed\n", 19, failures);
    printf("RESULT: %s\n", (failures == 0) ? "ALL TESTS PASSED" : "TEST FAILURES DETECTED");

    return (failures == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
