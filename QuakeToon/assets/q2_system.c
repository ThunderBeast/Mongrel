
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "../qcommon/qcommon.h"

/*
 * ============
 * va
 *
 * does a varargs printf into a temp buffer, so I don't need to have
 * varargs versions of all text functions.
 * FIXME: make this buffer size safe someday
 * ============
 */
char *va(char *format, ...)
{
    va_list     argptr;
    static char string[1024];

    va_start(argptr, format);
    vsprintf(string, format, argptr);
    va_end(argptr);

    return string;
}


void Sys_Error(char *error, ...)
{
    va_list argptr;

    printf("Sys_Error: ");
    va_start(argptr, error);
    vprintf(error, argptr);
    va_end(argptr);
    printf("\n");

    abort();

    exit(1);
}

int Sys_Milliseconds(void)
{
    return (int) SDL_GetTicks();
}


void Sys_Mkdir(char *path)
{
}


char *Sys_FindFirst(char *path, unsigned musthave, unsigned canthave)
{
    return NULL;
}


char *Sys_FindNext(unsigned musthave, unsigned canthave)
{
    return NULL;
}


void Sys_FindClose(void)
{
}


void Sys_Init(void)
{
}

// define this to dissalow any data but the demo pak file
//#define	NO_ADDONS

// if a packfile directory differs from this, it is assumed to be hacked
// Full version
#define PAK0_CHECKSUM    0x40e614e0
// Demo
//#define	PAK0_CHECKSUM	0xb2c6d7ea
// OEM
//#define	PAK0_CHECKSUM	0x78e135c

/*
 * =============================================================================
 *
 * QUAKE FILESYSTEM
 *
 * =============================================================================
 */


//
// in memory
//

typedef struct
{
    char name[MAX_QPATH];
    int  filepos, filelen;
} packfile_t;

typedef struct pack_s
{
    char       filename[MAX_OSPATH];
    FILE       *handle;
    int        numfiles;
    packfile_t *files;
} pack_t;

char   fs_gamedir[MAX_OSPATH];

const char *fs_basedir = "baseq2";
const char* fs_cddir = "";
cvar_t *fs_gamedirvar;

typedef struct filelink_s
{
    struct filelink_s *next;
    char              *from;
    int               fromlength;
    char              *to;
} filelink_t;

filelink_t *fs_links;

typedef struct searchpath_s
{
    char                filename[MAX_OSPATH];
    pack_t              *pack;  // only one of filename / pack will be used
    struct searchpath_s *next;
} searchpath_t;

searchpath_t *fs_searchpaths;
searchpath_t *fs_base_searchpaths;      // without gamedirs


/*
 *
 * All of Quake's data access is through a hierchal file system, but the contents of the file system can be transparently merged from several sources.
 *
 * The "base directory" is the path to the directory holding the quake.exe and all game directories.  The sys_* files pass this to host_init in quakeparms_t->basedir.  This can be overridden with the "-basedir" command line parm to allow code debugging in a different directory.  The base directory is
 * only used during filesystem initialization.
 *
 * The "game directory" is the first tree on the search path and directory that all generated files (savegames, screenshots, demos, config files) will be saved to.  This can be overridden with the "-game" command line parameter.  The game directory can never be changed while quake is executing.  This is a precacution against having a malicious server instruct clients to write files over areas they shouldn't.
 *
 */


/*
 * ================
 * FS_filelength
 * ================
 */
int FS_filelength(FILE *f)
{
    int pos;
    int end;

    pos = ftell(f);
    fseek(f, 0, SEEK_END);
    end = ftell(f);
    fseek(f, pos, SEEK_SET);

    return end;
}


/*
 * ============
 * FS_CreatePath
 *
 * Creates any directories needed to store the given filename
 * ============
 */
void FS_CreatePath(char *path)
{
    char *ofs;

    for (ofs = path + 1; *ofs; ofs++)
    {
        if (*ofs == '/')
        {               // create the directory
            *ofs = 0;
            Sys_Mkdir(path);
            *ofs = '/';
        }
    }
}


/*
 * ==============
 * FS_FCloseFile
 *
 * For some reason, other dll's can't just cal fclose()
 * on files returned by FS_FOpenFile...
 * ==============
 */
void FS_FCloseFile(FILE *f)
{
    fclose(f);
}


// RAFAEL

/*
 *      Developer_searchpath
 */
int Developer_searchpath(int who)
{
    int ch;
    // PMM - warning removal
//	char	*start;
    searchpath_t *search;

    if (who == 1)     // xatrix
    {
        ch = 'x';
    }
    else if (who == 2)
    {
        ch = 'r';
    }

    for (search = fs_searchpaths; search; search = search->next)
    {
        if (strstr(search->filename, "xatrix"))
        {
            return 1;
        }

        if (strstr(search->filename, "rogue"))
        {
            return 2;
        }

/*
 *              start = strchr (search->filename, ch);
 *
 *              if (start == NULL)
 *                      continue;
 *
 *              if (strcmp (start ,"xatrix") == 0)
 *                      return (1);
 */
    }
    return(0);
}


/*
 * ===========
 * FS_FOpenFile
 *
 * Finds the file in the search path.
 * returns filesize and an open FILE *
 * Used for streaming data out of either a pak file or
 * a seperate file.
 * ===========
 */
int file_from_pak = 0;
#ifndef NO_ADDONS
int FS_FOpenFile(char *filename, FILE **file)
{
    searchpath_t *search;
    char         netpath[MAX_OSPATH];
    pack_t       *pak;
    int          i;
    filelink_t   *link;

    file_from_pak = 0;

    // check for links first
    for (link = fs_links; link; link = link->next)
    {
        if (!strncmp(filename, link->from, link->fromlength))
        {
            sprintf(netpath, sizeof(netpath), "%s%s", link->to, filename + link->fromlength);
            *file = fopen(netpath, "rb");
            if (*file)
            {
                printf("link file: %s\n", netpath);
                return FS_filelength(*file);
            }
            return -1;
        }
    }

//
// search through the path, one element at a time
//
    for (search = fs_searchpaths; search; search = search->next)
    {
        // is the element a pak file?
        if (search->pack)
        {
            // look through all the pak file elements
            pak = search->pack;
            for (i = 0; i < pak->numfiles; i++)
            {
                if (!strcasecmp(pak->files[i].name, filename))
                {                       // found it!
                    file_from_pak = 1;
                    printf("PackFile: %s : %s\n", pak->filename, filename);
                    // open a new file on the pakfile
                    *file = fopen(pak->filename, "rb");
                    if (!*file)
                    {
                        Sys_Error( "Couldn't reopen %s", pak->filename);
                    }
                    fseek(*file, pak->files[i].filepos, SEEK_SET);
                    return pak->files[i].filelen;
                }
            }
        }
        else
        {
            // check a file in the directory tree

            sprintf(netpath, sizeof(netpath), "%s/%s", search->filename, filename);

            *file = fopen(netpath, "rb");
            if (!*file)
            {
                continue;
            }

            printf("FindFile: %s\n", netpath);

            return FS_filelength(*file);
        }
    }

    printf("FindFile: can't find %s\n", filename);

    *file = NULL;
    return -1;
}


#else

// this is just for demos to prevent add on hacking

int FS_FOpenFile(char *filename, FILE **file)
{
    searchpath_t *search;
    char         netpath[MAX_OSPATH];
    pack_t       *pak;
    int          i;

    file_from_pak = 0;

    // get config from directory, everything else from pak
    if (!strcmp(filename, "config.cfg") || !strncmp(filename, "players/", 8))
    {
        sprintf(netpath, sizeof(netpath), "%s/%s", FS_Gamedir(), filename);

        *file = fopen(netpath, "rb");
        if (!*file)
        {
            return -1;
        }

        printf("FindFile: %s\n", netpath);

        return FS_filelength(*file);
    }

    for (search = fs_searchpaths; search; search = search->next)
    {
        if (search->pack)
        {
            break;
        }
    }
    if (!search)
    {
        *file = NULL;
        return -1;
    }

    pak = search->pack;
    for (i = 0; i < pak->numfiles; i++)
    {
        if (!strcasecmp(pak->files[i].name, filename))
        {               // found it!
            file_from_pak = 1;
            printf("PackFile: %s : %s\n", pak->filename, filename);
            // open a new file on the pakfile
            *file = fopen(pak->filename, "rb");
            if (!*file)
            {
                Sys_Error( "Couldn't reopen %s", pak->filename);
            }
            fseek(*file, pak->files[i].filepos, SEEK_SET);
            return pak->files[i].filelen;
        }
    }

    printf("FindFile: can't find %s\n", filename);

    *file = NULL;
    return -1;
}
#endif


/*
 * =================
 * FS_ReadFile
 *
 * Properly handles partial reads
 * =================
 */


#define MAX_READ    0x10000             // read in blocks of 64k
void FS_Read(void *buffer, int len, FILE *f)
{
    int  block, remaining;
    int  read;
    byte *buf;
    int  tries;

    buf = (byte *)buffer;

    // read in chunks for progress bar
    remaining = len;
    tries     = 0;
    while (remaining)
    {
        block = remaining;
        if (block > MAX_READ)
        {
            block = MAX_READ;
        }
        read = fread(buf, 1, block, f);
        if (read == 0)
        {
            // we might have been trying to read from a CD
            if (!tries)
            {
                tries = 1;
                
            }
            else
            {
                Sys_Error( "FS_Read: 0 bytes read");
            }
        }

        if (read == -1)
        {
            Sys_Error( "FS_Read: -1 bytes read");
        }

        // do some progress bar thing here...

        remaining -= read;
        buf       += read;
    }
}


/*
 * ============
 * FS_LoadFile
 *
 * Filename are reletive to the quake search path
 * a null buffer will just return the file length without loading
 * ============
 */
int FS_LoadFile(char *path, void **buffer)
{
    FILE *h;
    byte *buf;
    int  len;

    buf = NULL;         // quiet compiler warning

// look for it in the filesystem or pack files
    len = FS_FOpenFile(path, &h);
    if (!h)
    {
        if (buffer)
        {
            *buffer = NULL;
        }
        return -1;
    }

    if (!buffer)
    {
        fclose(h);
        return len;
    }

    buf     = Z_Malloc(len);
    *buffer = buf;

    FS_Read(buf, len, h);

    fclose(h);

    return len;
}


/*
 * =============
 * FS_FreeFile
 * =============
 */
void FS_FreeFile(void *buffer)
{
    Z_Free(buffer);
}


/*
 * =================
 * FS_LoadPackFile
 *
 * Takes an explicit (not game tree related) path to a pak file.
 *
 * Loads the header and directory, adding the files at the beginning
 * of the list so they override previous pack files.
 * =================
 */
pack_t *FS_LoadPackFile(char *packfile)
{
    dpackheader_t header;
    int           i;
    packfile_t    *newfiles;
    int           numpackfiles;
    pack_t        *pack;
    FILE          *packhandle;
    dpackfile_t   info[MAX_FILES_IN_PACK];

    packhandle = fopen(packfile, "rb");
    if (!packhandle)
    {
        return NULL;
    }

    fread(&header, 1, sizeof(header), packhandle);
    if (LittleLong(header.ident) != IDPAKHEADER)
    {
        Sys_Error( "%s is not a packfile", packfile);
    }
    header.dirofs = LittleLong(header.dirofs);
    header.dirlen = LittleLong(header.dirlen);

    numpackfiles = header.dirlen / sizeof(dpackfile_t);

    if (numpackfiles > MAX_FILES_IN_PACK)
    {
        Sys_Error( "%s has %i files", packfile, numpackfiles);
    }

    newfiles = Z_Malloc(numpackfiles * sizeof(packfile_t));

    fseek(packhandle, header.dirofs, SEEK_SET);
    fread(info, 1, header.dirlen, packhandle);

// parse the directory
    for (i = 0; i < numpackfiles; i++)
    {
        strcpy(newfiles[i].name, info[i].name);
        newfiles[i].filepos = LittleLong(info[i].filepos);
        newfiles[i].filelen = LittleLong(info[i].filelen);
    }

    pack = Z_Malloc(sizeof(pack_t));
    strcpy(pack->filename, packfile);
    pack->handle   = packhandle;
    pack->numfiles = numpackfiles;
    pack->files    = newfiles;

    printf("Added packfile %s (%i files)\n", packfile, numpackfiles);
    return pack;
}


/*
 * ================
 * FS_AddGameDirectory
 *
 * Sets fs_gamedir, adds the directory to the head of the path,
 * then loads and adds pak1.pak pak2.pak ...
 * ================
 */
void FS_AddGameDirectory(char *dir)
{
    int          i;
    searchpath_t *search;
    pack_t       *pak;
    char         pakfile[MAX_OSPATH];

    strcpy(fs_gamedir, dir);

    //
    // add the directory to the search path
    //
    search = Z_Malloc(sizeof(searchpath_t));
    strcpy(search->filename, dir);
    search->next   = fs_searchpaths;
    fs_searchpaths = search;

    //
    // add any pak files in the format pak0.pak pak1.pak, ...
    //
    for (i = 0; i < 10; i++)
    {
        sprintf(pakfile, sizeof(pakfile), "%s/pak%i.pak", dir, i);
        pak = FS_LoadPackFile(pakfile);
        if (!pak)
        {
            continue;
        }
        search         = Z_Malloc(sizeof(searchpath_t));
        search->pack   = pak;
        search->next   = fs_searchpaths;
        fs_searchpaths = search;
    }
}


/*
 * ============
 * FS_Gamedir
 *
 * Called to find where to write a file (demos, savegames, etc)
 * ============
 */
char *FS_Gamedir(void)
{
    if (*fs_gamedir)
    {
        return fs_gamedir;
    }
    else
    {
        return BASEDIRNAME;
    }
}


/*
 * =============
 * FS_ExecAutoexec
 * =============
 */
void FS_ExecAutoexec(void)
{
    char *dir;
    char name [MAX_QPATH];

    dir = "baseq2";//Cvar_VariableString("gamedir");
    if (*dir)
    {
        sprintf(name, sizeof(name), "%s/%s/autoexec.cfg", fs_basedir, dir);
    }
    else
    {
        sprintf(name, sizeof(name), "%s/%s/autoexec.cfg", fs_basedir, BASEDIRNAME);
    }
    if (Sys_FindFirst(name, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM))
    {
        
    }
    Sys_FindClose();
}


/*
 * ================
 * FS_SetGamedir
 *
 * Sets the gamedir and path to a different directory.
 * ================
 */
void FS_SetGamedir(char *dir)
{
    searchpath_t *next;

    if (strstr(dir, "..") || strstr(dir, "/") ||
        strstr(dir, "\\") || strstr(dir, ":"))
    {
        printf("Gamedir should be a single filename, not a path\n");
        return;
    }

    //
    // free up any current game dir info
    //
    while (fs_searchpaths != fs_base_searchpaths)
    {
        if (fs_searchpaths->pack)
        {
            fclose(fs_searchpaths->pack->handle);
            Z_Free(fs_searchpaths->pack->files);
            Z_Free(fs_searchpaths->pack);
        }
        next = fs_searchpaths->next;
        Z_Free(fs_searchpaths);
        fs_searchpaths = next;
    }

    //
    // flush all data, so it will be forced to reload
    //
    if (dedicated && !dedicated->value)
    {
        
    }

    sprintf(fs_gamedir, sizeof(fs_gamedir), "%s/%s", fs_basedir, dir);

    if (!strcmp(dir, BASEDIRNAME) || (*dir == 0))
    {
        //Cvar_FullSet("gamedir", "", CVAR_SERVERINFO | CVAR_NOSET);
        //Cvar_FullSet("game", "", CVAR_LATCH | CVAR_SERVERINFO);
    }
    else
    {
        FS_AddGameDirectory(va("%s/%s", fs_basedir, dir));
    }
}

/*
** FS_ListFiles
*/
char **FS_ListFiles(char *findname, int *numfiles, unsigned musthave, unsigned canthave)
{
    char *s;
    int  nfiles = 0;
    char **list = 0;

    s = Sys_FindFirst(findname, musthave, canthave);
    while (s)
    {
        if (s[strlen(s) - 1] != '.')
        {
            nfiles++;
        }
        s = Sys_FindNext(musthave, canthave);
    }
    Sys_FindClose();

    if (!nfiles)
    {
        return NULL;
    }

    nfiles++;     // add space for a guard
    *numfiles = nfiles;

    list = malloc(sizeof(char *) * nfiles);
    memset(list, 0, sizeof(char *) * nfiles);

    s      = Sys_FindFirst(findname, musthave, canthave);
    nfiles = 0;
    while (s)
    {
        if (s[strlen(s) - 1] != '.')
        {
            list[nfiles] = strdup(s);
#ifdef _WIN32
            strlwr(list[nfiles]);
#endif
            nfiles++;
        }
        s = Sys_FindNext(musthave, canthave);
    }
    Sys_FindClose();

    return list;
}


/*
 * ============
 * FS_Path_f
 *
 * ============
 */
void FS_Path_f(void)
{
    searchpath_t *s;
    filelink_t   *l;

    printf("Current search path:\n");
    for (s = fs_searchpaths; s; s = s->next)
    {
        if (s == fs_base_searchpaths)
        {
            printf("----------\n");
        }
        if (s->pack)
        {
            printf("%s (%i files)\n", s->pack->filename, s->pack->numfiles);
        }
        else
        {
            printf("%s\n", s->filename);
        }
    }

    printf("\nLinks:\n");
    for (l = fs_links; l; l = l->next)
    {
        printf("%s : %s\n", l->from, l->to);
    }
}


/*
 * ================
 * FS_NextPath
 *
 * Allows enumerating all of the directories in the search path
 * ================
 */
char *FS_NextPath(char *prevpath)
{
    searchpath_t *s;
    char         *prev;

    if (!prevpath)
    {
        return fs_gamedir;
    }

    prev = fs_gamedir;
    for (s = fs_searchpaths; s; s = s->next)
    {
        if (s->pack)
        {
            continue;
        }
        if (prevpath == prev)
        {
            return s->filename;
        }
        prev = s->filename;
    }

    return NULL;
}


/*
 * ================
 * FS_InitFilesystem
 * ================
 */
void FS_InitFilesystem(void)
{
    //
    // basedir <path>
    // allows the game to run from outside the data tree
    //
    //fs_basedir = Cvar_Get("basedir", ".", CVAR_NOSET);

    //
    // cddir <path>
    // Logically concatenates the cddir after the basedir for
    // allows the game to run from outside the data tree
    //
    //fs_cddir = Cvar_Get("cddir", "", CVAR_NOSET);
    //if (fs_cddir->string[0])
    //{
    //    FS_AddGameDirectory(va("%s/"BASEDIRNAME, fs_cddir->string));
    //}

    //
    // start up with baseq2 by default
    //
    FS_AddGameDirectory(va("%s/"BASEDIRNAME, fs_basedir));

    // any set gamedirs will be freed up to here
    fs_base_searchpaths = fs_searchpaths;

    // check for game override
    //fs_gamedirvar = Cvar_Get("game", "", CVAR_LATCH | CVAR_SERVERINFO);
    //if (fs_gamedirvar->string[0])
    //{
    //    FS_SetGamedir(fs_gamedirvar->string);
    //}
}

/*
 * ==============================================================================
 *
 *                                              ZONE MEMORY ALLOCATION
 *
 * just cleared malloc with counters now...
 *
 * ==============================================================================
 */

#define Z_MAGIC    0x1d1d


typedef struct zhead_s
{
    struct zhead_s *prev, *next;
    short          magic;
    short          tag;                 // for group free
    int            size;
} zhead_t;

zhead_t z_chain;
int     z_count, z_bytes;

/*
 * ========================
 * Z_Free
 * ========================
 */
void Z_Free(void *ptr)
{
    zhead_t *z;

    z = ((zhead_t *)ptr) - 1;

    if (z->magic != Z_MAGIC)
    {
        Sys_Error( "Z_Free: bad magic");
    }

    z->prev->next = z->next;
    z->next->prev = z->prev;

    z_count--;
    z_bytes -= z->size;
    free(z);
}


/*
 * ========================
 * Z_Stats_f
 * ========================
 */
void Z_Stats_f(void)
{
    printf("%i bytes in %i blocks\n", z_bytes, z_count);
}


/*
 * ========================
 * Z_FreeTags
 * ========================
 */
void Z_FreeTags(int tag)
{
    zhead_t *z, *next;

    for (z = z_chain.next; z != &z_chain; z = next)
    {
        next = z->next;
        if (z->tag == tag)
        {
            Z_Free((void *)(z + 1));
        }
    }
}


/*
 * ========================
 * Z_TagMalloc
 * ========================
 */
void *Z_TagMalloc(int size, int tag)
{
    zhead_t *z;

    size = size + sizeof(zhead_t);
    z    = malloc(size);
    if (!z)
    {
        Sys_Error( "Z_Malloc: failed on allocation of %i bytes", size);
    }
    memset(z, 0, size);
    z_count++;
    z_bytes += size;
    z->magic = Z_MAGIC;
    z->tag   = tag;
    z->size  = size;

    z->next            = z_chain.next;
    z->prev            = &z_chain;
    z_chain.next->prev = z;
    z_chain.next       = z;

    return (void *)(z + 1);
}


/*
 * ========================
 * Z_Malloc
 * ========================
 */
void *Z_Malloc(int size)
{
    return Z_TagMalloc(size, 0);
}

double sqrt(double x);

vec_t VectorLength(vec3_t v)
{
    int   i;
    float length;

    length = 0;
    for (i = 0; i < 3; i++)
    {
        length += v[i] * v[i];
    }
    length = sqrt(length);              // FIXME

    return length;
}

/*
 * ============================================================================
 *
 *                                      BYTE ORDER FUNCTIONS
 *
 * ============================================================================
 */

qboolean bigendien;

// can't just use function pointers, or dll linkage can
// mess up when qcommon is included in multiple places
short (*_BigShort) (short l);
short (*_LittleShort) (short l);
int   (*_BigLong) (int l);
int   (*_LittleLong) (int l);
float (*_BigFloat) (float l);
float (*_LittleFloat) (float l);

short BigShort(short l)
{
    return _BigShort(l);
}


short LittleShort(short l)
{
    return _LittleShort(l);
}


int BigLong(int l)
{
    return _BigLong(l);
}


int LittleLong(int l)
{
    return _LittleLong(l);
}


float BigFloat(float l)
{
    return _BigFloat(l);
}


float LittleFloat(float l)
{
    return _LittleFloat(l);
}


short ShortSwap(short l)
{
    byte b1, b2;

    b1 = l & 255;
    b2 = (l >> 8) & 255;

    return (b1 << 8) + b2;
}


short ShortNoSwap(short l)
{
    return l;
}


int LongSwap(int l)
{
    byte b1, b2, b3, b4;

    b1 = l & 255;
    b2 = (l >> 8) & 255;
    b3 = (l >> 16) & 255;
    b4 = (l >> 24) & 255;

    return ((int)b1 << 24) + ((int)b2 << 16) + ((int)b3 << 8) + b4;
}


int LongNoSwap(int l)
{
    return l;
}


float FloatSwap(float f)
{
    union
    {
        float f;
        byte  b[4];
    }
    dat1, dat2;


    dat1.f    = f;
    dat2.b[0] = dat1.b[3];
    dat2.b[1] = dat1.b[2];
    dat2.b[2] = dat1.b[1];
    dat2.b[3] = dat1.b[0];
    return dat2.f;
}


float FloatNoSwap(float f)
{
    return f;
}


/*
 * ================
 * Swap_Init
 * ================
 */
void Swap_Init(void)
{
    byte swaptest[2] = { 1, 0 };

// set the byte swapping variables in a portable manner
    if (*(short *)swaptest == 1)
    {
        bigendien    = false;
        _BigShort    = ShortSwap;
        _LittleShort = ShortNoSwap;
        _BigLong     = LongSwap;
        _LittleLong  = LongNoSwap;
        _BigFloat    = FloatSwap;
        _LittleFloat = FloatNoSwap;
    }
    else
    {
        bigendien    = true;
        _BigShort    = ShortNoSwap;
        _LittleShort = ShortSwap;
        _BigLong     = LongNoSwap;
        _LittleLong  = LongSwap;
        _BigFloat    = FloatNoSwap;
        _LittleFloat = FloatSwap;
    }
}




