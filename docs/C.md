##C interface
As part of making the MatLab and Python interface we have also made a C interface to the library. The MatLab and Python interfaces make all their calls through this interface, but you can still use it if you want to.
The function names are subject to change (and probably will, because we want to camelCase the function names), and the interface currently pollutes your namespace quite a bit.
If you do want to use it, however, note that almost all functions emulate object oriented languages by taking a obj_ptr as the first argument, and then the rest of the arguments after that.
obj_ptr is currently defined as
```c
typedef void *obj_ptr;
```

```c
#include <stdio.h>
#include <stdlib.h>
#include <cinterface.h>

double f(double x, double y)
{
        return x*x*y + y*y*y;
}

int main(int argc, char **argv)
{
        obj_ptr datatable = datatable_init();
        printf("%s\n", get_error_string());

        int x_grid = 10;
        int y_grid = 10;
        int n_samples = x_grid * y_grid;

        double *samples = (double *) malloc(sizeof(double) * n_samples * 3);
        int sampleIdx = -1;
        for (int x = 0; x < x_grid; ++x)
        {
                for (int y = 0; y < y_grid; ++y)
                {
                        samples[++sampleIdx] = x;
                        samples[++sampleIdx] = y;
                        samples[++sampleIdx] = f(x, y);
                }
        }

        datatable_add_samples_row_major(datatable, samples, n_samples, 2);
        printf("%s\n", get_error_string());

        obj_ptr bspline = bspline_init(datatable, 3);
        printf("%s\n", get_error_string());

        double x_eval[] = {0.1, 0.5};
        double *val = eval_row_major(bspline, x_eval, 2);
        printf("%s\n", get_error_string());

        printf("Approximated value at (%f, %f): %f\n", x_eval[0], x_eval[1], val[0]);
        printf("Exact value at (%f, %f): %f\n", x_eval[0], x_eval[1], f(x_eval[0], x_eval[1]));

        return 0;
}

/* Output:
No error.
No error.
No error.
No error.
Approximated value at (0.100000, 0.500000): 0.130000
Exact value at (0.100000, 0.500000): 0.130000
*/
```
Example compiled with: c99 test.c -I../../include/ -L. -lsplinter-1-4 && ./a.out
from ~/SPLINTER/build/release, where libsplinter-1-4.so was located.

If you run into problems with running the produced executable, you should have a look [here](http://tldp.org/HOWTO/Program-Library-HOWTO/shared-libraries.html).
