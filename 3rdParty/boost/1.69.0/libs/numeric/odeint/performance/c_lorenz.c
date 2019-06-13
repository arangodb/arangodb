#include <stdio.h>
#include <time.h>
#include <math.h>

void lorenz(const double *x, double *restrict y) {
    y[0] = 10.0 * (x[1] - x[0]);
    y[1] = 28.0 * x[0] - x[1] - x[0] * x[2];
    y[2] = x[0] * x[1] - (8.0 / 3.0) * x[2];
}

int main(int argc, const char *argv[])
{
    const int nb_steps = 20000000;
    const double h = 1.0e-10;
    const double h2 = 0.5 * h;
    const double nb_loops = 21;
    double x[3];
    double y[3];
    double f1[3];
    double f2[3];
    double f3[3];
    double f4[3];
    double min_time = 1E6;
    clock_t begin, end;
    double time_spent;
    
    for (int j = 0; j < nb_loops; j++) {
        x[0] = 8.5;
        x[1] = 3.1;
        x[2] = 1.2;
        begin = clock();
        for (int k = 0; k < nb_steps; k++) {
            lorenz(x, f1);
            for (int i = 0; i < 3; i++) {
                y[i] = x[i] + h2 * f1[i];
            }
            lorenz(y, f2);
            for (int i = 0; i < 3; i++) {
                y[i] = x[i] + h2 * f2[i];
            }
            lorenz(y, f3);
            for (int i = 0; i < 3; i++) {
                y[i] = x[i] + h * f3[i];
            }
            lorenz(y, f4);
            for (int i = 0; i < 3; i++) {
                x[i] = x[i] + h * (f1[i] + 2 * (f2[i] + f3[i]) + f4[i]) / 6.0;
            }
        }
        end = clock();
        min_time = fmin(min_time, (double)(end-begin)/CLOCKS_PER_SEC);
        printf("Result: %f\t runtime: %f\n", x[0], (double)(end-begin)/CLOCKS_PER_SEC);
    }
    printf("Minimal Runtime: %f\n", min_time);
    
    return 0;
}
