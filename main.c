#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct Complex {
    double real;
    double imag;
};

struct Matrix {
    int rows;
    int cols;
    struct Complex **data;
};

struct ThreadData {
    const struct Matrix *a;
    const struct Matrix *b;
    struct Matrix *c;
    int start_row;
    int end_row;
};

void free_matrix(struct Matrix *matrix)
{
    if (matrix->data == NULL) {
        return;
    }

    for (int row = 0; row < matrix->rows; row++) {
        free(matrix->data[row]);
    }

    free(matrix->data);
    matrix->data = NULL;
}

int create_matrix(struct Matrix *matrix, int rows, int cols)
{
    matrix->rows = rows;
    matrix->cols = cols;

    if ((size_t)rows > SIZE_MAX / sizeof(struct Complex *) ||
        (size_t)cols > SIZE_MAX / sizeof(struct Complex)) {
        return 0;
    }

    matrix->data = calloc((size_t)rows, sizeof(struct Complex *));
    if (matrix->data == NULL) {
        return 0;
    }

    for (int row = 0; row < rows; row++) {
        matrix->data[row] = malloc((size_t)cols * sizeof(struct Complex));
        if (matrix->data[row] == NULL) {
            return 0;
        }
    }

    return 1;
}

int read_matrix(struct Matrix *matrix)
{
    int rows;
    int cols;

    if (scanf("%d %d", &rows, &cols) != 2 || rows < 1 || cols < 1) {
        return 0;
    }

    if (!create_matrix(matrix, rows, cols)) {
        return 0;
    }

    for (int row = 0; row < matrix->rows; row++) {
        for (int col = 0; col < matrix->cols; col++) {
            if (scanf("%lf %lf", &matrix->data[row][col].real,
                      &matrix->data[row][col].imag) != 2) {
                return 0;
            }

            if (!isfinite(matrix->data[row][col].real) ||
                !isfinite(matrix->data[row][col].imag)) {
                return 0;
            }
        }
    }

    return 1;
}

void print_matrix(const struct Matrix *matrix)
{
    for (int row = 0; row < matrix->rows; row++) {
        for (int col = 0; col < matrix->cols; col++) {
            printf("(%.12g%+.12gi) ", matrix->data[row][col].real,
                   matrix->data[row][col].imag);
        }
        printf("\n");
    }
}

void multiply_rows(const struct Matrix *a, const struct Matrix *b,
                   struct Matrix *c, int start_row, int end_row)
{
    for (int row = start_row; row < end_row; row++) {
        for (int col = 0; col < b->cols; col++) {
            c->data[row][col].real = 0;
            c->data[row][col].imag = 0;

            for (int k = 0; k < a->cols; k++) {
                struct Complex product;

                product.real = a->data[row][k].real * b->data[k][col].real
                             - a->data[row][k].imag * b->data[k][col].imag;

                product.imag = a->data[row][k].real * b->data[k][col].imag
                             + a->data[row][k].imag * b->data[k][col].real;

                c->data[row][col].real += product.real;
                c->data[row][col].imag += product.imag;
            }
        }
    }
}

void *worker(void *argument)
{
    struct ThreadData *data = argument;

    multiply_rows(data->a, data->b, data->c, data->start_row, data->end_row);

    return NULL;
}

int multiply_parallel(const struct Matrix *a, const struct Matrix *b,
                      struct Matrix *c, int thread_count)
{
    if (thread_count == 1) {
        multiply_rows(a, b, c, 0, a->rows);
        return 1;
    }

    if ((size_t)thread_count > SIZE_MAX / sizeof(struct ThreadData) ||
        (size_t)(thread_count - 1) > SIZE_MAX / sizeof(pthread_t)) {
        return 0;
    }

    pthread_t *threads = malloc((size_t)(thread_count - 1) * sizeof(pthread_t));
    struct ThreadData *tasks = malloc((size_t)thread_count * sizeof(struct ThreadData));

    if (threads == NULL || tasks == NULL) {
        free(threads);
        free(tasks);
        return 0;
    }

    int rows_per_thread = a->rows / thread_count;
    int extra_rows = a->rows % thread_count;
    int next_row = 0;

    for (int index = 0; index < thread_count; index++) {
        tasks[index].a = a;
        tasks[index].b = b;
        tasks[index].c = c;
        tasks[index].start_row = next_row;

        next_row += rows_per_thread;
        if (index < extra_rows) {
            next_row++;
        }

        tasks[index].end_row = next_row;
    }

    int created = 0;
    int create_error = 0;

    for (int index = 0; index < thread_count - 1; index++) {
        create_error = pthread_create(&threads[index], NULL, worker, &tasks[index]);
        if (create_error != 0) {
            break;
        }
        created++;
    }

    if (create_error == 0) {
        struct ThreadData *main_task = &tasks[thread_count - 1];
        multiply_rows(a, b, c, main_task->start_row, main_task->end_row);
    }

    for (int index = 0; index < created; index++) {
        int join_error = pthread_join(threads[index], NULL);
        if (join_error != 0) {
            fprintf(stderr, "Ошибка ожидания потока: %s\n", strerror(join_error));
            exit(EXIT_FAILURE);
        }
    }

    free(threads);
    free(tasks);

    if (create_error != 0) {
        fprintf(stderr, "Ошибка создания потока: %s\n", strerror(create_error));
        return 0;
    }

    return 1;
}

int read_thread_limit(const char *text)
{
    char *end;
    errno = 0;
    long value = strtol(text, &end, 10);

    if (errno != 0 || end == text || *end != '\0' || value < 1 || value > INT_MAX) {
        return 0;
    }

    return (int)value;
}

double get_time(void)
{
    struct timespec time;

    if (clock_gettime(CLOCK_MONOTONIC, &time) != 0) {
        perror("Не удалось измерить время");
        exit(EXIT_FAILURE);
    }

    return (double)time.tv_sec + (double)time.tv_nsec / 1000000000.0;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Запуск: %s число_потоков\n", argv[0]);
        return EXIT_FAILURE;
    }

    int thread_limit = read_thread_limit(argv[1]);
    if (thread_limit == 0) {
        fprintf(stderr, "Количество потоков должно быть целым числом от 1 до %d.\n", INT_MAX);
        return EXIT_FAILURE;
    }

    struct Matrix a = {0};
    struct Matrix b = {0};
    struct Matrix c = {0};

    printf("Введите размеры первой матрицы (строки столбцы), затем её элементы по строкам.\n"
           "Элемент: действительная и мнимая части.\n");
    fflush(stdout);

    if (!read_matrix(&a)) {
        fprintf(stderr, "Не удалось прочитать первую матрицу или выделить память.\n");
        free_matrix(&a);
        return EXIT_FAILURE;
    }

    printf("Введите размеры второй матрицы (строки столбцы), затем её элементы по строкам:\n");
    fflush(stdout);

    if (!read_matrix(&b)) {
        fprintf(stderr, "Не удалось прочитать вторую матрицу или выделить память.\n");
        free_matrix(&a);
        free_matrix(&b);
        return EXIT_FAILURE;
    }

    if (a.cols != b.rows) {
        fprintf(stderr, "Число столбцов первой матрицы должно равняться числу строк второй.\n");
        free_matrix(&a);
        free_matrix(&b);
        return EXIT_FAILURE;
    }

    if (!create_matrix(&c, a.rows, b.cols)) {
        fprintf(stderr, "Не удалось выделить память для результата.\n");
        free_matrix(&a);
        free_matrix(&b);
        free_matrix(&c);
        return EXIT_FAILURE;
    }

    int thread_count = thread_limit;
    if (thread_count > a.rows) {
        thread_count = a.rows;
    }

    double start = get_time();
    int success = multiply_parallel(&a, &b, &c, thread_count);
    double elapsed = get_time() - start;

    if (success) {
        printf("Время умножения: %.9f с\n", elapsed);
        printf("Результат:\n");
        print_matrix(&c);
    } else {
        fprintf(stderr, "Не удалось выполнить многопоточное умножение.\n");
    }

    free_matrix(&a);
    free_matrix(&b);
    free_matrix(&c);

    if (!success) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
