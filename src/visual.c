/*
 * Copyright 2018, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * visual.c -- visualization for debug and eyecandy purposes
 */

#include <stdio.h>
#ifndef _WIN32
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#else
#include <windows.h>
#include <io.h>
#endif

#include "libvmemcache.h"
#include "visual.h"

static unsigned int sx = 0, sy = 0; /* terminal size */

/*
 * vmemcache_visual_enable -- turn on visualization of vmemcache operations
 *
 * Returns recommended size of the cache, in blocks of arbitrary size.  If all
 * operations are aligned to blocks of that size, the display will accurately
 * depict the cache's contents.
 */
size_t
vmemcache_visual_enable()
{
#ifndef _WIN32
	struct winsize ts;

	if (!isatty(1))
		return 0;
	/* serial consoles often report size of 0x0 */
	if (ioctl(1, TIOCGWINSZ, &ts) || ts.ws_row <= 0 || ts.ws_col <= 0)
		return 0;

	sx = ts.ws_col;
	sy = ts.ws_row;
#else
	CONSOLE_SCREEN_BUFFER_INFO bi;
	HANDLE h;

	if (!(h = (HANDLE)_get_osfhandle(1)))
		return 0;
	if (!GetConsoleScreenBufferInfo(h, &bi))
		return 0;
	sx = bi.srWindow.Right-bi.srWindow.Left;
	sy = bi.srWindow.Bottom-bi.srWindow.Top;
#endif

	return sx * sy;
}

void
vmemcache_visual_draw(VMEMcache *c, void *addr, size_t len, uint64_t id)
{
	if (!sx)
		return;

	size_t offset = (size_t)((char *)addr - (char *)c->addr);
	size_t a1 = offset * sx * sy / c->size;
	size_t a2 = (offset + len) * sx * sy / c->size;
	if (a2 <= a1)
		return;

	/*
	 * Modern terminals implement three distinct ways to set color; those
	 * provide 16, 256 and 24-bit palettes.  24-bit is shoehorned to 256
	 * by xterm, and causes visual corruption on very old terminals (like
	 * some shipped with RHEL7).  Thus, let's pick 256.
	 * Colors 0 and 16 are black, and some other values in the first 16
	 * are redundant with later ones, thus let's skip 0..16 entirely.
	 */
	if (id)
		id = 17 + id % 239;

	/* goto (1-based, y then x); set background, write spaces, reset attr */
	printf("\e[%lu;%luf\e[48;5;%lum%*s\e[0m", a1 / sx + 1, a1 % sx + 1, id,
		(int)(a2 - a1), "");
	fflush(stdout);
}

uint64_t
vmemcache_visual_id(struct cache_entry *e)
{
	size_t ksize = e->key.ksize;
	uint64_t id = 0xcbf29ce484222325;

	for (size_t i = 0; i < ksize; i++) {
		id ^= (unsigned char)e->key.key[i];
		id *= 0x00000100000001b3;
	}

	return id;
}
