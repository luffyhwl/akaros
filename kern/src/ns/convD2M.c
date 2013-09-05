//#define DEBUG
#include <vfs.h>
#include <kfs.h>
#include <slab.h>
#include <kmalloc.h>
#include <kref.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <error.h>
#include <cpio.h>
#include <pmap.h>
#include <smp.h>
#include <fcall.h>

unsigned int sizeD2M(struct dir *d)
{
	char *sv[4];
	int i, ns;

	sv[0] = d->name;
	sv[1] = d->uid;
	sv[2] = d->gid;
	sv[3] = d->muid;

	ns = 0;
	for (i = 0; i < 4; i++)
		if (sv[i])
			ns += strlen(sv[i]);

	return STATFIXLEN + ns;
}

unsigned int convD2M(struct dir *d, uint8_t * buf, unsigned int nbuf)
{
	uint8_t *p, *ebuf;
	char *sv[4];
	int i, ns, nsv[4], ss;
	printd("%s: d %d buf %p, nbuf %d\n", __func__, d, buf, nbuf);
	if (nbuf < BIT16SZ)
		return 0;

	p = buf;
	ebuf = buf + nbuf;
	printd(">>>>>>>>>>>>>>>>>>>>>>convD2M: name: %s\n", d->name);
	sv[0] = d->name;
	sv[1] = NULL;	//d->uid;
	sv[2] = NULL;	//d->gid;
	sv[3] = NULL;	//d->muid;

	ns = 0;
	for (i = 0; i < 4; i++) {
		if (sv[i])
			nsv[i] = strlen(sv[i]);
		else
			nsv[i] = 0;
		printd("nsv [%d] = %d\n", i, nsv[i]);
		ns += nsv[i];
	}

	ss = STATFIXLEN + ns;
	/* set size before erroring, so user can know how much is needed */
	/* note that length excludes count field itself */
	PBIT16(p, ss - BIT16SZ);
	p += BIT16SZ;

	if (ss > nbuf)
		return BIT16SZ;
	PBIT16(p, d->type);
	p += BIT16SZ;
	PBIT32(p, d->dev);
	p += BIT32SZ;
	PBIT8(p, d->qid.type);
	p += BIT8SZ;
	PBIT32(p, d->qid.vers);
	p += BIT32SZ;
	PBIT64(p, d->qid.path);
	p += BIT64SZ;
	PBIT32(p, d->mode);
	p += BIT32SZ;
	PBIT32(p, d->atime);
	p += BIT32SZ;
	PBIT32(p, d->mtime);
	p += BIT32SZ;
	PBIT64(p, d->length);
	p += BIT64SZ;
	for (i = 0; i < 4; i++) {
		ns = nsv[i];
		if (p + ns + BIT16SZ > ebuf)
			return 0;
		PBIT16(p, ns);
		p += BIT16SZ;
		if (ns)
			memmove(p, sv[i], ns);
		p += ns;
	}

	if (ss != p - buf)
		return 0;
	return p - buf;
}