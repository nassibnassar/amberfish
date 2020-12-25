#ifndef _AF_STEM_H
#define _AF_STEM_H

#include <string.h>

#ifdef HAVE_STEMMER

#include <stdio.h>  /*  TEMPORARY  */

int stem(char *p, int i, int j);

static inline int af_stem(unsigned char *s)
{
	int end = stem((char *)s, 0, strlen((const char*)s) - 1) + 1;
	s[end] = '\0';
	return end;
}

static inline int af_stem_available()
{
	return 1;
}

#else

static inline int af_stem(unsigned char *s)
{
	return strlen((const char*)s);
}

static inline int af_stem_available()
{
	return 0;
}

#endif

#endif
