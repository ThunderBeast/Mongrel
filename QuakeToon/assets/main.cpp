#include <stdio.h>

extern "C"
{

void Qcommon_Init();

void R_BeginRegistration(char *model);
void DumpLightmapsToPng();

}

int main(int argc, char **argv)
{
	Qcommon_Init();

	R_BeginRegistration("base1");

	DumpLightmapsToPng();

	return 0;
}