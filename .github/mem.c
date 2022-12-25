#include <stdlib.h>
#include <unistd.h>

int main()
{
	int size = 1024 * 1024 * 10;
	int *array;
	for (int i = 0; i < size; i++) {
		array = malloc(sizeof(int));
		*array = i;
	}
	sleep(100);
}
