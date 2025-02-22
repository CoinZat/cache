/*-
 * Copyright (c) 2013-2015 Varnish Software AS
 * All rights reserved.
 *
 * Author: Poul-Henning Kamp <phk@phk.freebsd.dk>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

struct req;
struct vfp_entry;
struct vfp_ctx;
struct vdp_ctx;

/* Fetch processors --------------------------------------------------*/

enum vfp_status {
	VFP_ERROR = -1,
	VFP_OK = 0,
	VFP_END = 1,
	VFP_NULL = 2,	// signal bypass, never returned by VFP_Suck()
};

typedef enum vfp_status vfp_init_f(VRT_CTX, struct vfp_ctx *,
    struct vfp_entry *);
typedef enum vfp_status
    vfp_pull_f(struct vfp_ctx *, struct vfp_entry *, void *ptr, ssize_t *len);
typedef void vfp_fini_f(struct vfp_ctx *, struct vfp_entry *);

struct vfp {
	const char		*name;
	vfp_init_f		*init;
	vfp_pull_f		*pull;
	vfp_fini_f		*fini;
	const void		*priv1;
};

struct vfp_entry {
	unsigned		magic;
#define VFP_ENTRY_MAGIC		0xbe32a027
	enum vfp_status		closed;
	const struct vfp	*vfp;
	void			*priv1;
	intptr_t		priv2;
	VTAILQ_ENTRY(vfp_entry)	list;
	uint64_t		calls;
	uint64_t		bytes_out;
};

/*--------------------------------------------------------------------
 * VFP filter state
 */

VTAILQ_HEAD(vfp_entry_s, vfp_entry);

struct vfp_ctx {
	unsigned		magic;
#define VFP_CTX_MAGIC		0x61d9d3e5
	int			failed;
	struct http		*req;
	struct http		*resp;
	struct worker		*wrk;
	struct objcore		*oc;

	struct vfp_entry_s	vfp;
	struct vfp_entry	*vfp_nxt;
	unsigned		obj_flags;
};

enum vfp_status VFP_Suck(struct vfp_ctx *, void *p, ssize_t *lp);
enum vfp_status VFP_Error(struct vfp_ctx *, const char *fmt, ...)
    v_printflike_(2, 3);

void v_deprecated_ VRT_AddVFP(VRT_CTX, const struct vfp *);
void v_deprecated_ VRT_RemoveVFP(VRT_CTX, const struct vfp *);

/* Deliver processors ------------------------------------------------*/

enum vdp_action {
	VDP_NULL,		/* Input buffer valid after call */
	VDP_FLUSH,		/* Input buffer will be invalidated */
	VDP_END,		/* Last buffer or after, implies VDP_FLUSH */
};

typedef int vdp_init_f(VRT_CTX, struct vdp_ctx *, void **priv,
    struct objcore *);
/*
 * Return value:
 *	negative:	Error - abandon delivery
 *	zero:		OK
 *	positive:	Don't push this VDP anyway
 */

typedef int vdp_fini_f(struct vdp_ctx *, void **priv);
typedef int vdp_bytes_f(struct vdp_ctx *, enum vdp_action, void **priv,
    const void *ptr, ssize_t len);

struct vdp {
	const char		*name;
	vdp_init_f		*init;
	vdp_bytes_f		*bytes;
	vdp_fini_f		*fini;
};

struct vdp_entry {
	unsigned		magic;
#define VDP_ENTRY_MAGIC		0x353eb781
	enum vdp_action		end;	// VDP_NULL or VDP_END
	const struct vdp	*vdp;
	void			*priv;
	VTAILQ_ENTRY(vdp_entry)	list;
	uint64_t		calls;
	uint64_t		bytes_in;
};

VTAILQ_HEAD(vdp_entry_s, vdp_entry);

struct vdp_ctx {
	unsigned		magic;
#define VDP_CTX_MAGIC		0xee501df7
	int			retval;
	uint64_t		bytes_done;
	struct vdp_entry_s	vdp;
	struct vdp_entry	*nxt;
	struct worker		*wrk;
	struct vsl_log		*vsl;
	struct req		*req;
};

int VDP_bytes(struct vdp_ctx *, enum vdp_action act, const void *, ssize_t);

void v_deprecated_ VRT_AddVDP(VRT_CTX, const struct vdp *);
void v_deprecated_ VRT_RemoveVDP(VRT_CTX, const struct vdp *);

/* Registry functions -------------------------------------------------*/
const char *VRT_AddFilter(VRT_CTX, const struct vfp *, const struct vdp *);
void VRT_RemoveFilter(VRT_CTX, const struct vfp *, const struct vdp *);
