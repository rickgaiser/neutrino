#ifndef ISO_CNF_H
#define ISO_CNF_H

// Reads SYSTEM.CNF from ISO image and copies it into given buffer
int read_system_cnf(const char *path, char *system_cnf_data, int bufSize);

#endif
