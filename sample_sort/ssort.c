#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>

#include "float_vec.h"
#include "barrier.h"
#include "utils.h"

int compare_floats(const void* a, const void* b) {
    float l = *(float*) a;
    float r = *(float*) b;
    return (l > r) - (l < r);
}

    void
qsort_floats(floats* xs)
{
    qsort(xs->data, xs->size, sizeof(float), compare_floats);
}

    floats*
sample(float* data, long size, int P)
{
    //sample 3 * (P-1) random numbers
    floats* randomNums = make_floats(0);
    for (int ii = 0; ii < 3*(P-1); ++ii) {
        int index = rand() % size;
        floats_push(randomNums, data[index]); 
    }
    //sort them
    qsort_floats(randomNums);
    floats* medians = make_floats(0);
    floats_push(medians, 0);
    for (int ii = 0; ii < P-1; ++ii) {
        floats_push(medians, randomNums->data[1+3*ii]);
    }
    floats_push(medians, INFINITY);
    free_floats(randomNums);
    return medians;
}

    void
sort_worker(int pnum, float* data, long size, int P, floats* samps, long* sizes, barrier* bb)
{
    floats* xs = make_floats(0);

    float min = samps->data[pnum];
    float max = samps->data[pnum+1];
    for (int ii = 0; ii < size; ++ii) {
        if (data[ii] >= min && data[ii] < max) {
            floats_push(xs, data[ii]);
        }
    } 

    printf("%d: start %.04f, count %ld\n", pnum, samps->data[pnum], xs->size);

    sizes[pnum] = xs->size;

    qsort_floats(xs);

    barrier_wait(bb);

    long inputMin = 0;
    long inputMax = 0;
    for (long ii = 0; ii <= pnum; ++ii) {
        if (ii != pnum) {
            inputMin += sizes[ii];
        }
        inputMax += sizes[ii];
    }

    for (long ii = inputMin; ii < inputMax; ++ii) {
        data[ii] = xs->data[ii-inputMin]; 
    }

    free_floats(xs);
}

    void
run_sort_workers(float* data, long size, int P, floats* samps, long* sizes, barrier* bb)
{
    pid_t kids[P];

    for (int ii = 0; ii < P; ++ii) {
        if (!(kids[ii] = fork())) {
            sort_worker(ii, data, size, P, samps, sizes, bb);
            exit(0);
        }
    }

    for (int ii = 0; ii < P; ++ii) {
        int rv = waitpid(kids[ii], 0, 0);
        check_rv(rv);
    }
}

    void
sample_sort(float* data, long size, int P, long* sizes, barrier* bb)
{
    floats* samps = sample(data, size, P);
    run_sort_workers(data, size, P, samps, sizes, bb);
    free_floats(samps);
}

    int
main(int argc, char* argv[])
{
    alarm(120);

    if (argc != 3) {
        printf("Usage:\n");
        printf("\t%s P data.dat\n", argv[0]);
        return 1;
    }

    const int P = atoi(argv[1]);
    const char* fname = argv[2];

    seed_rng();

    int rv;
    struct stat st;
    rv = stat(fname, &st);
    check_rv(rv);

    const int fsize = st.st_size;
    if (fsize < 8) {
        printf("File too small.\n");
        return 1;
    }

    int fd = open(fname, O_RDWR);
    check_rv(fd);

    void* file = mmap(NULL, fsize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FILE, fd, 0);
    if (file == MAP_FAILED) {
        printf("Bad mmap\n");
    }
    
    long count = *(long*) file;
    float* data = file+8;

    for (int ii = 0; ii < count; ++ii) {
        printf("%f", data[ii]);
    }

    long* sizes = mmap(NULL, P*sizeof(long), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    barrier* bb = make_barrier(P);

    sample_sort(data, count, P, sizes, bb);

    free_barrier(bb);

    munmap(sizes, P*sizeof(long));

    munmap(file, fsize);

    close(fd);

    return 0;
}

