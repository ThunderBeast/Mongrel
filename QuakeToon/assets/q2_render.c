#include "../render/gl_local.h"


// render crumbs for linker from refresh

model_t  *currentmodel = NULL;
image_t  *r_notexture = NULL;
int r_viewcluster;
int r_oldviewcluster;
model_t *r_worldmodel = NULL;

refexport_t GetRefAPI(refimport_t rimp)
{
    refexport_t re;
    return re;
}
