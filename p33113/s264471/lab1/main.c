#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/mman.h>
//#include <values.h>


const int A = 198 * 1024 * 1024;
const void *B = (void *) 0x1BFC91AB;
const int D = 133;
const int E = 149 * 1024 * 1024;
const int G = 97;
const int I = 50;

int nb_files;
int fl_dscrprts[2];
char *mem_area;
int sum = 0;
int sm = 0;


typedef struct all_thr_t {
    int id;
} all_thr;


typedef struct sum_thr_t {
    int id;
    int sum;
} sum_thr;


void *all_thread_func(void *thr) {
    all_thr *thrs = (all_thr *) thr;


    int thr_nb_bytes = A / D;

    int start = thrs->id * thr_nb_bytes;
    int random_data = open("/dev/urandom", O_RDONLY);
    if (random_data < 0) printf("No access to urandom!\n");
    else {
        ssize_t res = -1;
        while (res < 0) {
            if (thrs->id != (D - 1)) res = read(random_data, &mem_area[start], thr_nb_bytes);
            else res = read(random_data, &mem_area[start], A - (thr_nb_bytes * (D - 1)));
            if (res < 0) printf("Error while reading data from urandom!\n");
        }

    }
    close(random_data);
    pthread_exit(NULL);
}


void *sum_thread_func(void *thr) {
    sum_thr *thrs = (sum_thr *) thr;
    int sum = 0;
    //printf("%d", nb_files);
    for (int i = 0; i < nb_files; i++) {
        if (i == nb_files - 1) {
            int rd_block_size;
            int file_size = A - E * (nb_files - 1);

            rd_block_size = (thrs->id != I - 1) ? file_size / I : file_size - ((I - 1) * (file_size / I));
            char rd_data[rd_block_size];
            pread(fl_dscrprts[i], &rd_data, rd_block_size, (file_size / I) * thrs->id);
            for (int j = 0; j < sizeof(rd_data); j++) {
                sum += rd_data[j];
            }
            if (thrs->id == I - 1) {
                flock(fl_dscrprts[i], LOCK_UN);
                close(fl_dscrprts[i]);
            }
        } else {
            int data_size = (thrs->id != I - 1) ? E / I : E - ((I - 1) * (E / I));

            //printf("before rd_data");
            char rd_data[data_size];//[data_size];
            //printf("before pread done");
            pread(fl_dscrprts[i], &rd_data, data_size, (E / I) * thrs->id);
            //printf("pread done");
            for (int j = 0; j < sizeof(rd_data); j++) {
                sum += rd_data[j];
            }
            if (thrs->id == I - 1) {
                flock(fl_dscrprts[i], LOCK_UN);
                close(fl_dscrprts[i]);
            }
        }
    }
    thrs->sum = sum;
    pthread_exit(NULL);
}


void *save_to_file() {
    nb_files = (A % E == 0) ? A / E : A / E + 1;
    size_t wr = 0;
    size_t bytes_sm = 0;

    int j = 0;
    for (int i = 1; i < nb_files + 1; i++) {
        char filename[5];
        snprintf(filename,sizeof(filename), "file%d", i);

        int f = open(filename, O_RDWR | O_CREAT, 0666);
        int ptr = 0;
        while ((wr < E) && (bytes_sm < A)) {
            size_t wr_it;
            if (bytes_sm + G > A) {
                wr_it = write(f, &mem_area[ptr], A - bytes_sm);
                ptr += A - bytes_sm;

            } else if (wr + G > E) {
                wr_it = write(f, &mem_area[ptr], E - wr);
                ptr += E - wr;
            } else {
                wr_it = write(f, &mem_area[ptr], G);
                ptr += G;
            }


            if (wr_it != -1) {
                wr += wr_it;
                bytes_sm += wr_it;
            } else {
                break;
            }
            j++;

        }
        flock(f, LOCK_UN);
        wr = 0;
        close(f);
    }
    return 0;
}


int main() {

    char ch;
    while ((ch = getchar()) != '\n' && ch != EOF);

    printf("Выделяем область памяти...\n");
    mem_area = mmap(B, A,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS,
                    0, 0);
    if (mem_area == -1) printf("Памятьне выделена!\n");
    else if (mem_area != B) printf("Память выделена по другому адресу\n");


    while ((ch = getchar()) != '\n' && ch != EOF);
    printf("Заполняем область памяти...\n");
    pthread_t thrs[D];
    all_thr all_thrs[D];


    for (int i = 0; i < D; i++) {
        all_thrs[i].id = i;

        pthread_create(&thrs[i], NULL, all_thread_func, (void *) &all_thrs[i]);
    }
    for (int i = 0; i < D; i++)
        pthread_join(thrs[i], NULL);
    while ((ch = getchar()) != '\n' && ch != EOF);
    printf("Cохраняем область памяти в файлы...\n");
    save_to_file();

    while ((ch = getchar()) != '\n' && ch != EOF);
    printf("Деаллокация...\n");
    munmap(mem_area, A);

    while ((ch = getchar()) != '\n' && ch != EOF);
    printf("Подсчет суммы...");
    pthread_t thrs_sm[I];
    sum_thr sum_thrs[I];
    for (int i = 1; i < nb_files + 1; i++) {
        char filename[5];
        snprintf(filename,sizeof(filename), "file%d", i);
        int f = open(filename, O_RDWR);
        flock(f, LOCK_SH);
        fl_dscrprts[i - 1] = f;
    }

    for (int i = 0; i < I; i++) {
        sum_thrs[i].id = i;
        sum_thread_func((void *) &sum_thrs[i]);
    }

    for (int i = 0; i < I; i++) {
        pthread_join(thrs_sm[i], NULL);
    }
    while ((ch = getchar()) != '\n' && ch != EOF);
    for (int i = 0; i < I; i++) {
        sm = sum_thrs[i].sum;
    }

    return 0;
}


