#ifndef IOPRP_H
#define IOPRP_H

#include <stdint.h>

struct SModList; /* forward declaration */

typedef struct romdir_entry
{
    char name[10];
    uint16_t extinfo_size;
    uint32_t size;
} romdir_entry_t;

typedef struct extinfo {
    uint16_t value;        /* Only applicable for the version field type. */
    uint8_t  ext_length;   /* The length of data appended to the end of this entry. */
    uint8_t  type;
} extinfo_t;

enum EXTINFO_TYPE {
    EXTINFO_TYPE_DATE    = 1,
    EXTINFO_TYPE_VERSION,
    EXTINFO_TYPE_COMMENT,
    EXTINFO_TYPE_NULL    = 0x7F
};

/*
 * Full IOPRP image: RESET + ROMDIR + EXTINFO + CDVDMAN + CDVDFSV + EESYNC
 * Used when DVD emulation is active.
 */
struct ioprp_ext_full {
    extinfo_t reset_date_ext;
    uint32_t  reset_date;

    extinfo_t cdvdman_date_ext;
    uint32_t  cdvdman_date;
    extinfo_t cdvdman_version_ext;
    extinfo_t cdvdman_comment_ext;
    char      cdvdman_comment[12];

    extinfo_t cdvdfsv_date_ext;
    uint32_t  cdvdfsv_date;
    extinfo_t cdvdfsv_version_ext;
    extinfo_t cdvdfsv_comment_ext;
    char      cdvdfsv_comment[16];

    extinfo_t syncee_date_ext;
    uint32_t  syncee_date;
    extinfo_t syncee_version_ext;
    extinfo_t syncee_comment_ext;
    char      syncee_comment[8];
};
struct ioprp_img_full {
    romdir_entry_t romdir[7];
    struct ioprp_ext_full ext;
};
extern const struct ioprp_img_full ioprp_img_full;

/*
 * DVD-only IOPRP image: RESET + ROMDIR + EXTINFO + EESYNC
 * Used when DVD emulation is not active.
 */
struct ioprp_ext_dvd {
    extinfo_t reset_date_ext;
    uint32_t  reset_date;

    extinfo_t syncee_date_ext;
    uint32_t  syncee_date;
    extinfo_t syncee_version_ext;
    extinfo_t syncee_comment_ext;
    char      syncee_comment[8];
};
struct ioprp_img_dvd {
    romdir_entry_t romdir[5];
    struct ioprp_ext_dvd ext;
};
extern const struct ioprp_img_dvd ioprp_img_dvd;

void         print_extinfo(const uint8_t *data, uint32_t size);
void         print_romdir(const struct romdir_entry *romdir);

/*
 * Copy romdir_in into romdir_out, replacing any module that appears in ml
 * by IOPRP name (sIOPRP field).  Returns the total size of the new image.
 */
unsigned int patch_IOPRP_image(struct romdir_entry *romdir_out,
                               const struct romdir_entry *romdir_in,
                               struct SModList *ml);

#endif
