
#include "stb_image_write.h"
#include "../render/gl_local.h"


#define LIGHTMAP_BYTES          4


// can raise these
#define BLOCK_WIDTH             128
#define BLOCK_HEIGHT            128

#define MAX_LIGHTMAPS           128

int c_visible_lightmaps;
int c_visible_textures;

#define GL_LIGHTMAP_FORMAT    GL_RGBA

static float s_blocklights[34 * 34 * 3];

static byte all_lightmaps[MAX_LIGHTMAPS][4 * BLOCK_WIDTH * BLOCK_HEIGHT];

typedef struct
{
    int        current_lightmap_texture;

    msurface_t *lightmap_surfaces[MAX_LIGHTMAPS];

    int        allocated[BLOCK_WIDTH];

    // the lightmap texture data needs to be kept in
    // main memory so texsubimage can update properly
    byte lightmap_buffer[4 * BLOCK_WIDTH * BLOCK_HEIGHT];

} gllightmapstate_t;

static gllightmapstate_t gl_lms;

void DumpLightmapsToPng()
{
    int i;
    char filename[1024];

    for (i = 0; i < gl_lms.current_lightmap_texture; i++)
    {
        sprintf(filename, "LIGHTMAP%i.png", i);
        stbi_write_png(filename, BLOCK_WIDTH, BLOCK_HEIGHT, 4, &all_lightmaps[i], 4 * BLOCK_WIDTH);
    }

    sprintf(filename, "LIGHTMAP%i.png", gl_lms.current_lightmap_texture);
    stbi_write_png(filename, BLOCK_WIDTH, BLOCK_HEIGHT, 4, &gl_lms.lightmap_buffer, 4 * BLOCK_WIDTH);

}


// returns a texture number and the position inside it
static qboolean LM_AllocBlock(int w, int h, int *x, int *y)
{
    int i, j;
    int best, best2;

    best = BLOCK_HEIGHT;

    for (i = 0; i < BLOCK_WIDTH - w; i++)
    {
        best2 = 0;

        for (j = 0; j < w; j++)
        {
            if (gl_lms.allocated[i + j] >= best)
            {
                break;
            }
            if (gl_lms.allocated[i + j] > best2)
            {
                best2 = gl_lms.allocated[i + j];
            }
        }
        if (j == w)
        {               // this is a valid spot
            *x = i;
            *y = best = best2;
        }
    }

    if (best + h > BLOCK_HEIGHT)
    {
        return false;
    }

    for (i = 0; i < w; i++)
    {
        gl_lms.allocated[*x + i] = best + h;
    }

    return true;
}


static void LM_InitBlock(void)
{
    memset(gl_lms.allocated, 0, sizeof(gl_lms.allocated));
}

/*
 * ===============
 * R_BuildLightMap
 *
 * Combine and scale multiple lightmaps into the floating format in blocklights
 * ===============
 */
void R_BuildLightMap(msurface_t *surf, byte *dest, int stride)
{
    int          smax, tmax;
    int          r, g, b, a, max;
    int          i, j, size;
    byte         *lightmap;
    float        scale[4];
    int          nummaps;
    float        *bl;
    lightstyle_t *style;
    int          monolightmap;

    if (surf->texinfo->flags & (SURF_SKY | SURF_TRANS33 | SURF_TRANS66 | SURF_WARP))
    {
        Sys_Error("R_BuildLightMap called for non-lit surface");
    }

    smax = (surf->extents[0] >> 4) + 1;
    tmax = (surf->extents[1] >> 4) + 1;
    size = smax * tmax;
    if (size > (sizeof(s_blocklights) >> 4))
    {
        Sys_Error("Bad s_blocklights size");
    }

// set to full bright if no light data
    if (!surf->samples)
    {
        int maps;

        for (i = 0; i < size * 3; i++)
        {
            s_blocklights[i] = 255;
        }
        for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255;
             maps++)
        {
            style = &r_newrefdef.lightstyles[surf->styles[maps]];
        }
        goto store;
    }

    // count the # of maps
    for (nummaps = 0; nummaps < MAXLIGHTMAPS && surf->styles[nummaps] != 255;
         nummaps++)
    {
    }

    lightmap = surf->samples;

    // add all the lightmaps
    if (nummaps == 1)
    {
        int maps;

        for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255;
             maps++)
        {
            bl = s_blocklights;

            for (i = 0; i < 3; i++)
            {
                scale[i] = 1.0f;//gl_modulate->value * r_newrefdef.lightstyles[surf->styles[maps]].rgb[i];
            }

            if ((scale[0] == 1.0F) &&
                (scale[1] == 1.0F) &&
                (scale[2] == 1.0F))
            {
                for (i = 0; i < size; i++, bl += 3)
                {
                    bl[0] = lightmap[i * 3 + 0];
                    bl[1] = lightmap[i * 3 + 1];
                    bl[2] = lightmap[i * 3 + 2];
                }
            }
            else
            {
                for (i = 0; i < size; i++, bl += 3)
                {
                    bl[0] = lightmap[i * 3 + 0] * scale[0];
                    bl[1] = lightmap[i * 3 + 1] * scale[1];
                    bl[2] = lightmap[i * 3 + 2] * scale[2];
                }
            }
            lightmap += size * 3;                       // skip to next lightmap
        }
    }
    else
    {
        int maps;

        memset(s_blocklights, 0, sizeof(s_blocklights[0]) * size * 3);

        for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255;
             maps++)
        {
            bl = s_blocklights;

            for (i = 0; i < 3; i++)
            {
                scale[i] = 1.0f;//gl_modulate->value * r_newrefdef.lightstyles[surf->styles[maps]].rgb[i];
            }

            if ((scale[0] == 1.0F) &&
                (scale[1] == 1.0F) &&
                (scale[2] == 1.0F))
            {
                for (i = 0; i < size; i++, bl += 3)
                {
                    bl[0] += lightmap[i * 3 + 0];
                    bl[1] += lightmap[i * 3 + 1];
                    bl[2] += lightmap[i * 3 + 2];
                }
            }
            else
            {
                for (i = 0; i < size; i++, bl += 3)
                {
                    bl[0] += lightmap[i * 3 + 0] * scale[0];
                    bl[1] += lightmap[i * 3 + 1] * scale[1];
                    bl[2] += lightmap[i * 3 + 2] * scale[2];
                }
            }
            lightmap += size * 3;                       // skip to next lightmap
        }
    }

// add all the dynamic lights
    //if (surf->dlightframe == r_framecount)
    //{
    //    R_AddDynamicLights(surf);
    //}

// put into texture format
store:
    stride -= (smax << 2);
    bl      = s_blocklights;


    for (i = 0; i < tmax; i++, dest += stride)
    {
        for (j = 0; j < smax; j++)
        {
            r = Q_ftol(bl[0]);
            g = Q_ftol(bl[1]);
            b = Q_ftol(bl[2]);

            // catch negative lights
            if (r < 0)
            {
                r = 0;
            }
            if (g < 0)
            {
                g = 0;
            }
            if (b < 0)
            {
                b = 0;
            }

            /*
            ** determine the brightest of the three color components
            */
            if (r > g)
            {
                max = r;
            }
            else
            {
                max = g;
            }
            if (b > max)
            {
                max = b;
            }

            /*
            ** alpha is ONLY used for the mono lightmap case.  For this reason
            ** we set it to the brightest of the color components so that
            ** things don't get too dim.
            */
            a = max;

            /*
            ** rescale all the color components if the intensity of the greatest
            ** channel exceeds 1.0
            */
            if (max > 255)
            {
                float t = 255.0F / max;

                r = r * t;
                g = g * t;
                b = b * t;
                a = a * t;
            }

            dest[0] = r;
            dest[1] = g;
            dest[2] = b;
            dest[3] = a;

            bl   += 3;
            dest += 4;
        }
    }


}

/*
 * ========================
 * GL_CreateSurfaceLightmap
 * ========================
 */
void GL_CreateSurfaceLightmap(msurface_t *surf)
{
    int  smax, tmax;
    byte *base;

    if (surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
    {
        return;
    }

    smax = (surf->extents[0] >> 4) + 1;
    tmax = (surf->extents[1] >> 4) + 1;

    if (!LM_AllocBlock(smax, tmax, &surf->light_s, &surf->light_t))
    {	

    	// Q2Convert
        //LM_UploadBlock(false);

        memcpy(all_lightmaps[gl_lms.current_lightmap_texture], gl_lms.lightmap_buffer, sizeof(gl_lms.lightmap_buffer));

        if (++gl_lms.current_lightmap_texture == MAX_LIGHTMAPS)
        {
            Sys_Error("MAX_LIGHTMAPS exceeded\n");
        }

        LM_InitBlock();
        if (!LM_AllocBlock(smax, tmax, &surf->light_s, &surf->light_t))
        {
            Sys_Error("Consecutive calls to LM_AllocBlock(%d,%d) failed\n", smax, tmax);
        }
    }

    surf->lightmaptexturenum = gl_lms.current_lightmap_texture;

    base  = gl_lms.lightmap_buffer;
    base += (surf->light_t * BLOCK_WIDTH + surf->light_s) * LIGHTMAP_BYTES;

    //Q2Convert
    //R_SetCacheState(surf);
    R_BuildLightMap(surf, base, BLOCK_WIDTH * LIGHTMAP_BYTES);
}

/*
 * ================
 * GL_BuildPolygonFromSurface
 * ================
 */
void GL_BuildPolygonFromSurface(msurface_t *fa)
{
    int      i, lindex, lnumverts;
    medge_t  *pedges, *r_pedge;
    int      vertpage;
    float    *vec;
    float    s, t;
    glpoly_t *poly;
    vec3_t   total;

// reconstruct the polygon
    pedges    = currentmodel->edges;
    lnumverts = fa->numedges;
    vertpage  = 0;

    VectorClear(total);
    //
    // draw texture
    //
    poly           = Hunk_Alloc(sizeof(glpoly_t) + (lnumverts - 4) * VERTEXSIZE * sizeof(float));
    poly->next     = fa->polys;
    poly->flags    = fa->flags;
    fa->polys      = poly;
    poly->numverts = lnumverts;

    for (i = 0; i < lnumverts; i++)
    {
        lindex = currentmodel->surfedges[fa->firstedge + i];

        if (lindex > 0)
        {
            r_pedge = &pedges[lindex];
            vec     = currentmodel->vertexes[r_pedge->v[0]].position;
        }
        else
        {
            r_pedge = &pedges[-lindex];
            vec     = currentmodel->vertexes[r_pedge->v[1]].position;
        }
        s  = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
        //Q2Convert fix once we have image width
        s /= 64;//fa->texinfo->image->width;

        t  = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
        //Q2Convert fix once we have image height
        t /= 64;//fa->texinfo->image->height;

        VectorAdd(total, vec, total);
        VectorCopy(vec, poly->verts[i]);
        poly->verts[i][3] = s;
        poly->verts[i][4] = t;

        //
        // lightmap texture coordinates
        //
        s  = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
        s -= fa->texturemins[0];
        s += fa->light_s * 16;
        s += 8;
        s /= BLOCK_WIDTH * 16;       //fa->texinfo->texture->width;

        t  = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
        t -= fa->texturemins[1];
        t += fa->light_t * 16;
        t += 8;
        t /= BLOCK_HEIGHT * 16;       //fa->texinfo->texture->height;

        poly->verts[i][5] = s;
        poly->verts[i][6] = t;
    }

    poly->numverts = lnumverts;
}



/*
 * ==================
 * GL_BeginBuildingLightmaps
 *
 * ==================
 */
void GL_BeginBuildingLightmaps(model_t *m)
{
    static lightstyle_t lightstyles[MAX_LIGHTSTYLES];
    int                 i;
    unsigned            dummy[128 * 128];

    memset(gl_lms.allocated, 0, sizeof(gl_lms.allocated));

    memset(gl_lms.lightmap_buffer, 0, sizeof(gl_lms.lightmap_buffer));
    //byte* p = gl_lms.lightmap_buffer + 3;
    //while (p - gl_lms.lightmap_buffer < sizeof(gl_lms.lightmap_buffer))
    //{
    //    *p = 255;
    //    p += 4;
    //}

    /*
    ** setup the base lightstyles so the lightmaps won't have to be regenerated
    ** the first time they're seen
    */
    for (i = 0; i < MAX_LIGHTSTYLES; i++)
    {
        lightstyles[i].rgb[0] = 1;
        lightstyles[i].rgb[1] = 1;
        lightstyles[i].rgb[2] = 1;
        lightstyles[i].white  = 3;

    }

    gl_lms.current_lightmap_texture = 1;

}


/*
 * =======================
 * GL_EndBuildingLightmaps
 * =======================
 */
void GL_EndBuildingLightmaps(void)
{

}