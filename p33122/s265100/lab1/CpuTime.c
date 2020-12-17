#include <unistd.h>
#include <sys/resource.h>
#include <sys/times.h>
#include <time.h>

double getCPUTime() {

#if defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
#if defined(_POSIX_TIMERS) && (_POSIX_TIMERS > 0)
    {
        clockid_t id;
        struct timespec ts;
#if _POSIX_CPUTIME > 0
#endif
#if defined(CLOCK_PROCESS_CPUTIME_ID)
        id = CLOCK_PROCESS_CPUTIME_ID;
#elif defined(CLOCK_VIRTUAL)
#endif
        if (id != (clockid_t) -1 && clock_gettime(id, &ts) != -1)
            return (double) ts.tv_sec +
                   (double) ts.tv_nsec / 1000000000.0;
    }
#endif

#if defined(RUSAGE_SELF)
    {
        struct rusage rusage;
        if (getrusage(RUSAGE_SELF, &rusage) != -1)
            return (double) rusage.ru_utime.tv_sec +
                   (double) rusage.ru_utime.tv_usec / 1000000.0;
    }
#endif

#if defined(_SC_CLK_TCK)
    {
        const double ticks = (double) sysconf(_SC_CLK_TCK);
        struct tms tms;
        if (times(&tms) != (clock_t) -1)
            return (double) tms.tms_utime / ticks;
    }
#endif

#if defined(CLOCKS_PER_SEC)
    {
        clock_t cl = clock();
        if (cl != (clock_t) -1)
            return (double) cl / (double) CLOCKS_PER_SEC;
    }
#endif
#endif
    return -1;
}