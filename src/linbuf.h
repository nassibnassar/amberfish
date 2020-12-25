#ifndef _AF_LINBUF_H
#define _AF_LINBUF_H

int aflinbuf(FILE *f, int mem);
int aflinread(void *ptr, off_t offset, size_t size);

#endif
