#include <stdio.h>

extern "C"
{

void Qcommon_Init();

void R_BeginRegistration(char *model);

}

int main(int argc, char **argv)
{
	Qcommon_Init();

	R_BeginRegistration("base1");

	return 0;
}