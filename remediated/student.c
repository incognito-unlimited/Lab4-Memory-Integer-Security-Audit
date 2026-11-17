#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>
#include <stddef.h>

#define MAX_NAME 32
#define INITIAL_MARK_COUNT 5
#define INITIAL_RESIZE_COUNT 10

typedef struct {
    char *name;
    int *marks;
    size_t count;
} Student;

/* Reject counts whose byte size cannot be represented by size_t. */
static int checked_array_bytes(size_t count, size_t element_size, size_t *bytes)
{
    if ((bytes == NULL) || (element_size == 0u) ||
        (count > SIZE_MAX / element_size)) {
        return 0;
    }
    *bytes = count * element_size;
    return 1;
}

/* Safely determine a bounded string length without reading beyond MAX_NAME. */
static int bounded_name_length(const char *name, size_t *length)
{
    size_t index = 0u;

    if ((name == NULL) || (length == NULL)) {
        return 0;
    }
    while ((index <= (size_t)MAX_NAME) && (name[index] != '\0')) {
        index++;
    }
    if ((index == 0u) || (index > (size_t)MAX_NAME)) {
        return 0;
    }
    *length = index;
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

    /* Allocate the already-validated byte count as one zero-initialised block. */
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

void display_student(const Student *student)
{
    if ((student == NULL) || (student->name == NULL) ||
        ((student->count > 0u) && (student->marks == NULL))) {
        fprintf(stderr, "Invalid student record\n");
        return;
    }

    printf("Student: %s\n", student->name);
    for (size_t index = 0u; index < student->count; index++) {
        printf("Mark[%zu] = %d\n", index, student->marks[index]);
    }
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

#ifndef STUDENT_NO_MAIN
int main(void)
{
    static const int initial_marks[INITIAL_MARK_COUNT] = {80, 75, 90, 85, 95};
    Student *student = create_student("Alice", INITIAL_MARK_COUNT);
    int64_t total = 0;

    if (student == NULL) {
        fprintf(stderr, "Unable to create student\n");
        return EXIT_FAILURE;
    }

    for (size_t index = 0u; index < student->count; index++) {
        student->marks[index] = initial_marks[index];
    }

    display_student(student);

    if (!calculate_total(student, &total)) {
        fprintf(stderr, "Total calculation failed\n");
        destroy_student(&student);
        return EXIT_FAILURE;
    }
    printf("Total = %" PRId64 "\n", total);

    if (!add_bonus(student, 5)) {
        fprintf(stderr, "Bonus update failed\n");
        destroy_student(&student);
        return EXIT_FAILURE;
    }

    if (!resize_marks(student, INITIAL_RESIZE_COUNT)) {
        fprintf(stderr, "Resize failed\n");
        destroy_student(&student);
        return EXIT_FAILURE;
    }

    display_student(student);
    printf("First mark = %d\n", student->marks[0]);

    destroy_student(&student);
    return EXIT_SUCCESS;
}
#endif
