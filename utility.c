int generate_random_int_between(int min, int max, struct timeval tv)
{
    gettimeofday(&tv, NULL);
    // unsigned int seedr = time(NULL);
    return (rand_r(&tv.tv_usec) % (max - min + 1)) + min;
}
