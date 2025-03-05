/*
  Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
  Copyright 2006-2008 Polo
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.

  Some parts of the code are taken from HD Project by Polo
*/

// libc/newlib
#include <string.h>

// PS2SDK
#include <sifdma.h>
#include <sifcmd.h>

// Neutrino
#include "ee_debug.h"
#include "iopmgr.h"
#include "util.h"

/* Do not link to strcpy() from libc */
void _strcpy(char *dst, const char *src)
{
    strncpy(dst, src, strlen(src) + 1);
}

/* Do not link to strcat() from libc */
void _strcat(char *dst, const char *src)
{
    _strcpy(&dst[strlen(dst)], src);
}

/* Do not link to strncmp() from libc */
int _strncmp(const char *s1, const char *s2, int length)
{
    const char *a = s1;
    const char *b = s2;

    while (length > 0) {
        if ((*a == 0) || (*b == 0))
            return -1;
        if (*a++ != *b++)
            return 1;
        length--;
    }

    return 0;
}

/* Do not link to strcmp() from libc */
int _strcmp(const char *s1, const char *s2)
{
    int i = 0;

    while ((s1[i] != 0) && (s1[i] == s2[i]))
        i++;

    if (s1[i] > s2[i])
        return 1;
    else if (s1[i] < s2[i])
        return -1;

    return 0;
}

/* Do not link to strchr() from libc */
char *_strchr(const char *string, int c)
{
    while (*string) {
        if (*string == c)
            return (char *)string;
        string++;
    }

    if (*string == c)
        return (char *)string;

    return NULL;
}

/* Do not link to strrchr() from libc */
char *_strrchr(const char *string, int c)
{
    /* use the asm strchr to do strrchr */
    char *lastmatch;
    char *result;

    /* if char is never found then this will return 0 */
    /* if char is found then this will return the last matched location
       before strchr returned 0 */

    lastmatch = 0;
    result = _strchr(string, c);

    while ((int)result != 0) {
        lastmatch = result;
        result = _strchr(lastmatch + 1, c);
    }

    return lastmatch;
}

/* Do not link to strtok() from libc */
char *_strtok(char *strToken, const char *strDelimit)
{
    static char *start;
    static char *end;

    if (strToken != NULL)
        start = strToken;
    else {
        if (*end == 0)
            return 0;

        start = end;
    }

    if (*start == 0)
        return 0;

    // Strip out any leading delimiters
    while (_strchr(strDelimit, *start)) {
        // If a character from the delimiting string
        // then skip past it

        start++;

        if (*start == 0)
            return 0;
    }

    if (*start == 0)
        return 0;

    end = start;

    while (*end != 0) {
        if (_strchr(strDelimit, *end)) {
            // if we find a delimiting character
            // before the end of the string, then
            // terminate the token and move the end
            // pointer to the next character
            *end = 0;
            end++;
            return start;
        }
        end++;
    }

    // reached the end of the string before finding a delimiter
    // so dont move on to the next character
    return start;
}

/* Do not link to strstr() from libc */
char *_strstr(const char *string, const char *substring)
{
    char *strpos;

    if (string == 0)
        return 0;

    if (strlen(substring) == 0)
        return (char *)string;

    strpos = (char *)string;

    while (*strpos != 0) {
        if (_strncmp(strpos, substring, strlen(substring)) == 0)
            return strpos;

        strpos++;
    }

    return 0;
}

/* Do not link to islower() from libc */
int _islower(int c)
{
    if ((c < 'a') || (c > 'z'))
        return 0;

    // passed both criteria, so it
    // is a lower case alpha char
    return 1;
}

/* Do not link to toupper() from libc */
int _toupper(int c)
{
    if (_islower(c))
        c -= 32;

    return c;
}

/* Do not link to memcmp() from libc */
int _memcmp(const void *s1, const void *s2, unsigned int length)
{
    const char *a = s1;
    const char *b = s2;

    while (length--) {
        if (*a++ != *b++)
            return 1;
    }

    return 0;
}

/*----------------------------------------------------------------------------------------*/
/* This function converts string to unsigned integer. Stops on illegal characters.        */
/* Put here because including atoi rises the size of loader.elf by another kilobyte       */
/* and that causes some games to stop working.                                            */
/*----------------------------------------------------------------------------------------*/
unsigned int _strtoui(const char *p)
{
    if (!p)
        return 0;

    int r = 0;

    while (*p) {
        if ((*p < '0') || (*p > '9'))
            return r;

        r = r * 10 + (*p++ - '0');
    }

    return r;
}

/*----------------------------------------------------------------------------------------*/
/* This function converts string to signed integer. Stops on illegal characters.        */
/* Put here because including atoi rises the size of loader.elf by another kilobyte       */
/* and that causes some games to stop working.                                            */
/*----------------------------------------------------------------------------------------*/

int _strtoi(const char *p)
{
    int k = 1;
    if (!p)
        return 0;

    int r = 0;

    while (*p) {
        if (*p == '-') {
            k = -1;
            p++;
        } else if ((*p < '0') || (*p > '9'))
            return r;
        r = r * 10 + (*p++ - '0');
    }

    r = r * k;

    return r;
}

/*----------------------------------------------------------------------------------------*/
/* This function converts string to u64. Stops on illegal characters.                     */
/* Put here because including atoi rises the size of loader.elf by another kilobyte       */
/* and that causes some games to stop working.                                            */
/*----------------------------------------------------------------------------------------*/
u64 _strtoul(const char *p)
{
    if (!p)
        return 0;

    u64 r = 0;

    while (*p) {
        if ((*p < '0') || (*p > '9'))
            return r;

        r = r * 10 + (*p++ - '0');
    }

    return r;
}

/*----------------------------------------------------------------------------------------*/
/* This function retrieve a pattern in a buffer, using a mask                             */
/*----------------------------------------------------------------------------------------*/
u32 *find_pattern_with_mask(u32 *buf, unsigned int bufsize, const u32 *pattern, const u32 *mask, unsigned int len)
{
    unsigned int i, j;

    len /= sizeof(u32);
    bufsize /= sizeof(u32);

    for (i = 0; i < bufsize - len; i++) {
        for (j = 0; j < len; j++) {
            if ((buf[i + j] & mask[j]) != pattern[j])
                break;
        }
        if (j == len)
            return &buf[i];
    }

    return NULL;
}

/*----------------------------------------------------------------------------------------*/
/* Copy 'size' bytes of 'eedata' from EE to 'iopptr' in IOP.                              */
/*----------------------------------------------------------------------------------------*/
void CopyToIop(const void *eedata, unsigned int size, void *iopptr)
{
    SifDmaTransfer_t dmadata;
    int id;

    dmadata.src = (void *)eedata;
    dmadata.dest = iopptr;
    dmadata.size = size;
    dmadata.attr = 0;
    SifWriteBackDCache((void *)eedata, size);

    do {
        id = SifSetDma(&dmadata, 1);
    } while (!id);

    while (SifDmaStat(id) >= 0) {
        ;
    }
}

/*----------------------------------------------------------------------------------------*/
/* Initialize User Memory.                                                                */
/*----------------------------------------------------------------------------------------*/
void WipeUserMemory(void *start, void *end)
{
    unsigned int i;

    for (i = (unsigned int)start; i < (unsigned int)end; i += 64) {
        __asm__ __volatile__(
            "\tsq $0, 0(%0) \n"
            "\tsq $0, 16(%0) \n"
            "\tsq $0, 32(%0) \n"
            "\tsq $0, 48(%0) \n" ::"r"(i));
    }
}

// About 1ms
void delay(int count)
{
    int i, j;
    for (i = 0; i < count; i++) {
        j = 0x010000;
        while (j--)
            asm("nop\nnop\nnop\nnop");
    }
}

void BGERROR(u32 func_color, int count) {
    int i;
    while (1) {
        *GS_REG_BGCOLOR = func_color;
        delay(500);
        *GS_REG_BGCOLOR = COLOR_BLACK;
        delay(500);
        for (i=0; i<count; i++) {
            *GS_REG_BGCOLOR = COLOR_RED;
            delay(150);
            *GS_REG_BGCOLOR = COLOR_BLACK;
            delay(350);
        }
        delay(150);
    }
}
