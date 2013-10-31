/*
 * SHL - Ring buffer
 *
 * Copyright (c) 2011-2013 David Herrmann <dh.herrmann@gmail.com>
 * Dedicated to the Public Domain
 */

/*
 * Ring buffer
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include "shl_ring.h"

#define RING_MASK(_r, _v) ((_v) & ((_r)->size - 1))

/*
 * Resize ring-buffer to size @nsize. @nsize must be a power-of-2, otherwise
 * ring operations will behave incorrectly.
 */
static int ring_resize(struct shl_ring *r, size_t nsize)
{
	char *buf;

	buf = malloc(nsize);
	if (!buf)
		return -ENOMEM;

	if (r->end == r->start) {
		r->end = 0;
		r->start = 0;
	} else if (r->end > r->start) {
		memcpy(buf, &r->buf[r->start], r->end - r->start);

		r->end -= r->start;
		r->start = 0;
	} else {
		memcpy(buf, &r->buf[r->start], r->size - r->start);
		memcpy(&buf[r->size - r->start], r->buf, r->end);

		r->end += r->size - r->start;
		r->start = 0;
	}

	free(r->buf);
	r->buf = buf;
	r->size = nsize;

	return 0;
}

/* Compute next higher power-of-2 of @v. Returns 4096 in case v is 0. */
static size_t ring_pow2(size_t v)
{
	size_t i;

	if (!v)
		return 4096;

	--v;

	for (i = 1; i < 8 * sizeof(size_t); i *= 2)
		v |= v >> i;

	return ++v;
}

/*
 * Resize ring-buffer to provide enough room for @add bytes of new data. This
 * resizes the buffer if it is too small. It returns -ENOMEM on OOM and 0 on
 * success.
 */
static int ring_grow(struct shl_ring *r, size_t add)
{
	size_t len;

	/*
	 * Note that "end == start" means "empty buffer". Hence, we can never
	 * fill the last byte of a buffer. That means, we must account for an
	 * additional byte here ("end == start"-byte).
	 */

	if (r->end < r->start)
		len = r->start - r->end;
	else
		len = r->start + r->size - r->end;

	/* don't use ">=" as "end == start" would be ambigious */
	if (len > add)
		return 0;

	/* +1 for additional "end == start" byte */
	len = r->size + add - len + 1;
	len = ring_pow2(len);

	if (len <= r->size)
		return -ENOMEM;

	return ring_resize(r, len);
}

/*
 * Push @len bytes from @u8 into the ring buffer. The buffer is resized if it
 * is too small. -ENOMEM is returned on OOM, 0 on success.
 */
int shl_ring_push(struct shl_ring *r, const char *u8, size_t len)
{
	int err;
	size_t l;

	err = ring_grow(r, len);
	if (err < 0)
		return err;

	if (r->start <= r->end) {
		l = r->size - r->end;
		if (l > len)
			l = len;

		memcpy(&r->buf[r->end], u8, l);
		r->end = RING_MASK(r, r->end + l);

		len -= l;
		u8 += l;
	}

	if (!len)
		return 0;

	memcpy(&r->buf[r->end], u8, len);
	r->end = RING_MASK(r, r->end + len);

	return 0;
}

/*
 * Get data pointers for current ring-buffer data. @vec must be an array of 2
 * iovec objects. They are filled according to the data available in the
 * ring-buffer. 0, 1 or 2 is returned according to the number of iovec objects
 * that were filled (0 meaning buffer is empty).
 *
 * Hint: "struct iovec" is defined in <sys/uio.h> and looks like this:
 *     struct iovec {
 *         void *iov_base;
 *         size_t iov_len;
 *     };
 */
size_t shl_ring_peek(struct shl_ring *r, struct iovec *vec)
{
	if (r->end > r->start) {
		if (vec) {
			vec[0].iov_base = &r->buf[r->start];
			vec[0].iov_len = r->end - r->start;
		}
		return 1;
	} else if (r->end < r->start) {
		if (vec) {
			vec[0].iov_base = &r->buf[r->start];
			vec[0].iov_len = r->size - r->start;
			vec[1].iov_base = r->buf;
			vec[1].iov_len = r->end;
		}
		return r->end ? 2 : 1;
	} else {
		return 0;
	}
}

/*
 * Remove @len bytes from the start of the ring-buffer. Note that we protect
 * against overflows so removing more bytes than available is safe.
 */
void shl_ring_pull(struct shl_ring *r, size_t len)
{
	size_t l;

	if (r->start > r->end) {
		l = r->size - r->start;
		if (l > len)
			l = len;

		r->start = RING_MASK(r, r->start + l);
		len -= l;
	}

	if (!len)
		return;

	l = r->end - r->start;
	if (l > len)
		l = len;

	r->start = RING_MASK(r, r->start + l);
}

void shl_ring_flush(struct shl_ring *r)
{
	r->start = 0;
	r->end = 0;
}

void shl_ring_clear(struct shl_ring *r)
{
	free(r->buf);
	memset(r, 0, sizeof(*r));
}

char *shl_ring_copy(struct shl_ring *r, size_t *len)
{
	struct iovec vec[2];
	size_t n, sum;
	char *b;

	sum = 0;
	n = shl_ring_peek(r, vec);
	if (n > 0)
		sum += vec[0].iov_len;
	if (n > 1)
		sum += vec[1].iov_len;

	if (len && *len < sum)
		sum = *len;

	b = malloc(sum + 1);
	if (!b)
		return NULL;

	b[sum] = 0;
	if (len)
		*len = sum;

	if (n > 0) {
		if (vec[0].iov_len > sum)
			vec[0].iov_len = sum;

		memcpy(b, vec[0].iov_base, vec[0].iov_len);
		sum -= vec[0].iov_len;

		if (n > 1 && sum > 0) {
			if (vec[1].iov_len > sum)
				vec[1].iov_len = sum;

			memcpy(&b[vec[0].iov_len],
			       vec[1].iov_base, vec[1].iov_len);
		}
	}

	return b;
}
