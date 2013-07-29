#include <stdio.h>

extern "C"
{

void Qcommon_Init();

}

int main(int argc, char **argv)
{
	Qcommon_Init();
	return 0;
}