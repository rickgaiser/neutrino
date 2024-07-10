/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include "internal.h"

static void cdvdman_trimspaces(char *str);
static struct dirTocEntry *cdvdman_locatefile(char *name, u32 tocLBA, int tocLength, int layer);
static int cdvdman_findfile(sceCdlFILE *pcd_file, const char *name, int layer);

// The max lsn/sectors available based on value retrieved from iso. Used for out of bounds checking. Only check if value non zero.
u32 mediaLsnCount;

typedef struct
{
    u32 rootDirtocLBA;
    u32 rootDirtocLength;
} layer_info_t;

static layer_info_t layer_info[2];

//-------------------------------------------------------------------------
static void cdvdman_trimspaces(char *str)
{
    int i, len;
    char *p;

    len = strlen(str);
    if (len == 0)
        return;

    for (i = len - 1; i != -1; i--) {
        p = &str[i];
        if ((*p != 0x20) && (*p != 0x2e))
            break;
        *p = 0;
    }
}

//-------------------------------------------------------------------------
static struct dirTocEntry *cdvdman_locatefile(char *name, u32 tocLBA, int tocLength, int layer)
{
    char cdvdman_dirname[32]; /* Like below, but follow the original SCE limitation of 32-characters.
                        Some games specify filenames like "\\FILEFILE.EXT;1", which result in a length longer than just 14.
                        SCE does not perform bounds-checking on this buffer.	*/
    char cdvdman_curdir[15];  /* Maximum 14 characters: the filename (8) + '.' + extension (3) + ';' + '1'.
                        Unlike the SCE original which used a 32-character buffer,
                        we'll assume that ISO9660 disc images are all _strictly_ compliant with ISO9660 level 1.	*/
    char *p = (char *)name;
    char *slash;
    int r, len, filename_len;
    int tocPos;
    struct dirTocEntry *tocEntryPointer;

lbl_startlocate:
    //M_DEBUG("%s start locating %s, layer=%d\n", __FUNCTION__, name, layer);

    while (*p == '/')
        p++;

    while (*p == '\\')
        p++;

    slash = strchr(p, '/');

    // if the path doesn't contain a '/' then look for a '\'
    if (!slash)
        slash = strchr(p, '\\');

    len = (u32)slash - (u32)p;

    // if a slash was found
    if (slash != NULL) {
#ifdef DEBUG
        if (len >= sizeof(cdvdman_dirname)) {
            M_DEBUG("%s: segment too long: (%d chars) \"%s\"\n", __FUNCTION__, len, p);
            asm volatile("break\n");
        }
#endif

        // copy the path into main 'dir' var
        strncpy(cdvdman_dirname, p, len);
        cdvdman_dirname[len] = 0;
    } else {
#ifdef DEBUG
        len = strlen(p);

        if (len >= sizeof(cdvdman_dirname)) {
            M_DEBUG("%s: filename too long: (%d chars) \"%s\"\n", __FUNCTION__, len, p);
            asm volatile("break\n");
        }
#endif

        strcpy(cdvdman_dirname, p);
    }

    while (tocLength > 0) {
        if (sceCdRead_internal(tocLBA, 1, cdvdman_buf, NULL, ECS_SEARCHFILE) == 0)
            return NULL;
        sceCdSync(0);
        //M_DEBUG("%s tocLBA read done\n", __FUNCTION__);

        tocLength -= 2048;
        tocLBA++;

        tocPos = 0;
        do {
            tocEntryPointer = (struct dirTocEntry *)&cdvdman_buf[tocPos];

            if (tocEntryPointer->length == 0)
                break;

            filename_len = tocEntryPointer->filenameLength;
            if (filename_len) {
                strncpy(cdvdman_curdir, tocEntryPointer->filename, filename_len); // copy filename
                cdvdman_curdir[filename_len] = 0;

                //M_DEBUG("%s strcmp %s %s\n", __FUNCTION__, cdvdman_dirname, cdvdman_curdir);

                r = strncmp(cdvdman_dirname, cdvdman_curdir, 12);
                if ((!r) && (!slash)) { // we searched a file so it's found
                    //M_DEBUG("%s found file! LBA=%d size=%d\n", __FUNCTION__, (int)tocEntryPointer->fileLBA, (int)tocEntryPointer->fileSize);
                    return tocEntryPointer;
                } else if ((!r) && (tocEntryPointer->fileProperties & 2)) { // we found it but it's a directory
                    tocLBA = tocEntryPointer->fileLBA;
                    tocLength = tocEntryPointer->fileSize;
                    p = &slash[1];

                    if (!(cdvdman_settings.flags & IOPCORE_COMPAT_EMU_DVDDL)) {
                        int on_dual;
                        unsigned int layer1_start;
                        sceCdReadDvdDualInfo(&on_dual, &layer1_start);

                        if (layer)
                            tocLBA += layer1_start;
                    }

                    goto lbl_startlocate;
                } else {
                    tocPos += (tocEntryPointer->length << 16) >> 16;
                }
            }
        } while (tocPos < 2016);
    }

    //M_DEBUG("%s file not found!!!\n", __FUNCTION__);

    return NULL;
}

//-------------------------------------------------------------------------
static int cdvdman_findfile(sceCdlFILE *pcdfile, const char *name, int layer)
{
    static char cdvdman_filepath[256];
    u32 lsn;
    struct dirTocEntry *tocEntryPointer;
    layer_info_t *pLayerInfo;

    cdvdman_init();

    if (cdvdman_settings.flags & IOPCORE_COMPAT_EMU_DVDDL)
        layer = 0;
    pLayerInfo = (layer != 0) ? &layer_info[1] : &layer_info[0]; // SCE CDVDMAN simply treats a non-zero value as a signal for the 2nd layer.

    WaitSema(cdvdman_searchfilesema);

    M_DEBUG("%s %s layer%d\n", __FUNCTION__, name, layer);

    strncpy(cdvdman_filepath, name, sizeof(cdvdman_filepath));
    cdvdman_filepath[sizeof(cdvdman_filepath) - 1] = '\0';
    cdvdman_trimspaces(cdvdman_filepath);

    M_DEBUG("%s cdvdman_filepath=%s\n", __FUNCTION__, cdvdman_filepath);

    if (pLayerInfo->rootDirtocLBA == 0) {
        SignalSema(cdvdman_searchfilesema);
        return 0;
    }

    tocEntryPointer = cdvdman_locatefile(cdvdman_filepath, pLayerInfo->rootDirtocLBA, pLayerInfo->rootDirtocLength, layer);
    if (tocEntryPointer == NULL) {
        SignalSema(cdvdman_searchfilesema);
        return 0;
    }

    lsn = tocEntryPointer->fileLBA;
    if (layer) {
        sceCdReadDvdDualInfo((int *)&pcdfile->lsn, (unsigned int *)&pcdfile->size);
        lsn += pcdfile->size;
    }

    pcdfile->lsn = lsn;
    pcdfile->size = tocEntryPointer->fileSize;

    strcpy(pcdfile->name, strrchr(name, '\\') + 1);

    M_DEBUG("%s found %s\n", __FUNCTION__, name);

    SignalSema(cdvdman_searchfilesema);

    return 1;
}

//-------------------------------------------------------------------------
int sceCdSearchFile(sceCdlFILE *pcd_file, const char *name)
{
    M_DEBUG("%s %s\n", __FUNCTION__, name);

    return cdvdman_findfile(pcd_file, name, 0);
}

//-------------------------------------------------------------------------
int sceCdLayerSearchFile(sceCdlFILE *fp, const char *name, int layer)
{
    M_DEBUG("%s %s\n", __FUNCTION__, name);

    return cdvdman_findfile(fp, name, layer);
}

//-------------------------------------------------------------------------
void cdvdman_searchfile_init(void)
{
    // Read the volume descriptor
    sceCdRead_internal(16, 1, cdvdman_buf, NULL, ECS_SEARCHFILE);
    sceCdSync(0);

    struct dirTocEntry *tocEntryPointer = (struct dirTocEntry *)&cdvdman_buf[0x9c];
    layer_info[0].rootDirtocLBA = tocEntryPointer->fileLBA;
    layer_info[0].rootDirtocLength = tocEntryPointer->fileSize;

    // PVD Volume Space Size field
    //mediaLsnCount = *(u32 *)&cdvdman_buf[0x50];
    //M_DEBUG("cdvdman_searchfile_init mediaLsnCount=%d\n", mediaLsnCount);

    // DVD DL support
    if (!(cdvdman_settings.flags & IOPCORE_COMPAT_EMU_DVDDL)) {
        int on_dual;
        unsigned int layer1_start;
        sceCdReadDvdDualInfo(&on_dual, &layer1_start);
        if (on_dual) {
            //u32 lsn0 = mediaLsnCount;
            // So that CdRead below can read more than first layer.
            //mediaLsnCount = 0;
            sceCdRead_internal(layer1_start + 16, 1, cdvdman_buf, NULL, ECS_SEARCHFILE);
            sceCdSync(0);
            tocEntryPointer = (struct dirTocEntry *)&cdvdman_buf[0x9c];
            layer_info[1].rootDirtocLBA = layer1_start + tocEntryPointer->fileLBA;
            layer_info[1].rootDirtocLength = tocEntryPointer->fileSize;

            //u32 lsn1 = *(u32 *)&cdvdman_buf[0x50];
            //M_DEBUG("cdvdman_searchfile_init DVD9 L0 mediaLsnCount=%d \n", lsn0);
            //M_DEBUG("cdvdman_searchfile_init DVD9 L1 mediaLsnCount=%d \n", lsn1);
            //mediaLsnCount = lsn0 + lsn1 - 16;
            //M_DEBUG("cdvdman_searchfile_init DVD9 mediaLsnCount=%d\n", mediaLsnCount);
        }
    }
}
