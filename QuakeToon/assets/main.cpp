#include <stdio.h>

extern "C"
{

void Swap_Init(void);

}

int main(int argc, char **argv)
{
	Swap_Init();
	return 0;
}