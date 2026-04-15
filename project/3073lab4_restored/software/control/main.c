#include <stdio.h>
#include <unistd.h>

int main()
{
    while (1)
    {
        printf("Hello\n");
        usleep(500000); // 0.5 second delay
    }
    return 0;
}
