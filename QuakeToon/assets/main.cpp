#include <stdio.h>


extern "C"
{

void R_BeginRegistration(char *model);
void DumpLightmapsToPng();
void Qcommon_Init(int argc, char **argv);

void* Mod_ForName(char *name, bool crash);
void ExportLevelModel(void *model);

}


int main(int argc, char **argv)
{
	Qcommon_Init(argc, argv);

	R_BeginRegistration("base1");

	DumpLightmapsToPng();

	void* model = Mod_ForName((char*) "maps/base1.bsp", false);

	if (model)
		ExportLevelModel(model);


	return 0;
}