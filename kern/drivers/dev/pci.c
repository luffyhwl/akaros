/* 
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

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
#include <ip.h>
#include <arch/io.h>

enum {
	Qtopdir = 0,

	Qpcidir,
	Qpcictl,
	Qpciraw,
};

#define TYPE(q)		((uint32_t)(q).path & 0x0F)
#define QID(c, t)	(((c)<<4)|(t))

static struct dirtab topdir[] = {
	{".", {Qtopdir, 0, QTDIR}, 0, 0555},
	{"pci", {Qpcidir, 0, QTDIR}, 0, 0555},
};

extern struct dev pcidevtab;

static int pcidirgen(struct chan *c, int t, int tbdf, struct dir *dp)
{
	struct qid q;

	q = (struct qid) {
	BUSBDF(tbdf) | t, 0, 0};
	switch (t) {
		case Qpcictl:
			snprintf(get_cur_genbuf(), GENBUF_SZ, "%d.%d.%dctl",
					 BUSBNO(tbdf), BUSDNO(tbdf), BUSFNO(tbdf));
			devdir(c, q, get_cur_genbuf(), 0, eve, 0444, dp);
			return 1;
		case Qpciraw:
			snprintf(get_cur_genbuf(), GENBUF_SZ, "%d.%d.%draw",
					 BUSBNO(tbdf), BUSDNO(tbdf), BUSFNO(tbdf));
			devdir(c, q, get_cur_genbuf(), 128, eve, 0664, dp);
			return 1;
	}
	return -1;
}

static int
pcigen(struct chan *c, char *unused, struct dirtab *unuseddirtab, int unused_int, int s,
	   struct dir *dp)
{
	int tbdf;
	struct pci_device *p = NULL;
	struct qid q;

	switch (TYPE(c->qid)) {
		case Qtopdir:
			if (s == DEVDOTDOT) {
				q = (struct qid) {
				QID(0, Qtopdir), 0, QTDIR};
				snprintf(get_cur_genbuf(), GENBUF_SZ, "#%C", pcidevtab.dc);
				devdir(c, q, get_cur_genbuf(), 0, eve, 0555, dp);
				return 1;
			}
			return devgen(c, NULL, topdir, ARRAY_SIZE(topdir), s, dp);
		case Qpcidir:
			if (s == DEVDOTDOT) {
				q = (struct qid) {
				QID(0, Qtopdir), 0, QTDIR};
				snprintf(get_cur_genbuf(), GENBUF_SZ, "#%C", pcidevtab.dc);
				devdir(c, q, get_cur_genbuf(), 0, eve, 0555, dp);
				return 1;
			}
			p = pci_match_vd(NULL, 0, 0);
			while (s >= 2 && p != NULL) {
				p = pci_match_vd(p, 0, 0);
				s -= 2;
			}
			if (p == NULL)
				return -1;
			return pcidirgen(c, s + Qpcictl, MKBUS(0,p->bus,p->dev,p->func), dp);
		case Qpcictl:
		case Qpciraw:
			tbdf = MKBUS(BusPCI, 0, 0, 0) | BUSBDF((uint32_t) c->qid.path);
			p = pci_match_tbdf(tbdf);
			if (p == NULL)
				return -1;
			return pcidirgen(c, TYPE(c->qid), tbdf, dp);
		default:
			break;
	}
	return -1;
}

static struct chan *pciattach(char *spec)
{
	return devattach(pcidevtab.dc, spec);
}

struct walkqid *pciwalk(struct chan *c, struct chan *nc, char **name, int nname)
{
	return devwalk(c, nc, name, nname, (struct dirtab *)0, 0, pcigen);
}

static int pcistat(struct chan *c, uint8_t * dp, int n)
{
	return devstat(c, dp, n, (struct dirtab *)0, 0L, pcigen);
}

static struct chan *pciopen(struct chan *c, int omode)
{
	c = devopen(c, omode, (struct dirtab *)0, 0, pcigen);
	switch (TYPE(c->qid)) {
		default:
			break;
	}
	return c;
}

static void pciclose(struct chan *unused)
{
}

static long pciread(struct chan *c, void *va, long n, int64_t offset)
{
	char buf[256], *ebuf, *w, *a;
	int i, tbdf, r;
	uint32_t x;
	struct pci_device *p;

	a = va;
	switch (TYPE(c->qid)) {
		case Qtopdir:
		case Qpcidir:
			return devdirread(c, a, n, (struct dirtab *)0, 0L, pcigen);
		case Qpcictl:
			tbdf = MKBUS(BusPCI, 0, 0, 0) | BUSBDF((uint32_t) c->qid.path);
			p = pci_match_tbdf(tbdf);
			if (p == NULL)
				error(Egreg);
			ebuf = buf + sizeof buf - 1;	/* -1 for newline */
			w = seprintf(buf, ebuf, "%.2x.%.2x.%.2x %.4x/%.4x %3d",
						 p->class, p->subclass, p->progif, p->ven_id, p->dev_id, p->irqline);
			for (i = 0; i < p->nr_bars; i++) {
				w = seprintf(w, ebuf, " %d:%.8lux %d", i, p->bar[i].raw_bar,
					     p->bar[i].mmio_sz);
			}
			*w++ = '\n';
			*w = '\0';
			return readstr(offset, a, n, buf);
		case Qpciraw:
			tbdf = MKBUS(BusPCI, 0, 0, 0) | BUSBDF((uint32_t) c->qid.path);
			p = pci_match_tbdf(tbdf);
			if (p == NULL)
				error(Egreg);
			if (n + offset > 256)
				n = 256 - offset;
			if (n < 0)
				return 0;
			r = offset;
			if (!(r & 3) && n == 4) {
				x = pcidev_read32(p, r);
				PBIT32(a, x);
				return 4;
			}
			if (!(r & 1) && n == 2) {
				x = pcidev_read16(p, r);
				PBIT16(a, x);
				return 2;
			}
			for (i = 0; i < n; i++) {
				x = pcidev_read8(p, r);
				PBIT8(a, x);
				a++;
				r++;
			}
			return i;
		default:
			error(Egreg);
	}
	return n;
}

static long pciwrite(struct chan *c, void *va, long n, int64_t offset)
{
	char buf[256];
	uint8_t *a;
	int i, r, tbdf;
	uint32_t x;
	struct pci_device *p;

	if (n >= sizeof(buf))
		n = sizeof(buf) - 1;
	a = va;
	strncpy(buf, (char *)a, n);
	buf[n] = 0;

	switch (TYPE(c->qid)) {
		case Qpciraw:
			tbdf = MKBUS(BusPCI, 0, 0, 0) | BUSBDF((uint32_t) c->qid.path);
			p = pci_match_tbdf(tbdf);
			if (p == NULL)
				error(Egreg);
			if (offset > 256)
				return 0;
			if (n + offset > 256)
				n = 256 - offset;
			r = offset;
			if (!(r & 3) && n == 4) {
				x = GBIT32(a);
				pcidev_write32(p, r, x);
				return 4;
			}
			if (!(r & 1) && n == 2) {
				x = GBIT16(a);
				pcidev_write16(p, r, x);
				return 2;
			}
			for (i = 0; i < n; i++) {
				x = GBIT8(a);
				pcidev_write8(p, r, x);
				a++;
				r++;
			}
			return i;
		default:
			error(Egreg);
	}
	return n;
}

struct dev pcidevtab __devtab = {
	'$',
	"pci",

	devreset,
	devinit,
	devshutdown,
	pciattach,
	pciwalk,
	pcistat,
	pciopen,
	devcreate,
	pciclose,
	pciread,
	devbread,
	pciwrite,
	devbwrite,
	devremove,
	devwstat,
};
