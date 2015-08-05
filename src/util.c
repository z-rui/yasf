#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdarg.h>

struct buffer {
	size_t size;
	char bytes[1];
};

#define BUFFER(buf) ((struct buffer *)((buf) - offsetof(struct buffer, bytes)))

char *bufnew(size_t size)
{
	struct buffer *buf;

	buf = malloc(sizeof (struct buffer) + size);
	if (buf) {
		buf->size = size;
		buf->bytes[0] = 0;
		return buf->bytes;
	}
	return 0;
}

char *bufext(char **base, char *tail, size_t n)
{
	size_t len, len1, size;

	if (!tail) return 0;
	len = (size_t) (tail - *base);
	len1 = len + n;
	if ((size = BUFFER(*base)->size) < len1) {
		struct buffer *buffer;

		do
			size *= 2;
		while (size < len1);
		if ((buffer = realloc(BUFFER(*base), sizeof (struct buffer) + size))) {
			buffer->size = size;
			*base = buffer->bytes;
		} else {	/* OOM */
			return 0;
		}
	}
	/* defensive programming: put a sentinel zero right after the desired
	 * length, so that the buffer is always a legal C string and the user
	 * is free to use use string routines in standard library. */
	(*base)[len1] = 0;
	return tail = *base + len;
}

char *bufcat(char **base, char *tail, const char *s)
{
	size_t slen;

	if (!tail) return 0;
	slen = strlen(s);
	tail = bufext(base, tail, slen);
	if (tail) {
		memcpy(tail, s, slen);
		tail += slen;
	}
	return tail;
}

void buffree(char *buf)
{
	if (buf)
		free(BUFFER(buf));
}

size_t escquote(char *out, const char *in, int esc)
{
	if (out) {
		char *base = out;
		while (*in) {
			*out++ = *in;
			if (*in == esc)
				*out++ = *in;
			in++;
		}
		return (size_t) (out - base);
	} else {
		size_t sz = 0;
		while (*in) {
			sz += (*in == esc) + 1;
			in++;
		}
		return sz;
	}
}

static
char *bufcatQ(char **buf, char *p, const char *s)
{
	p = bufext(buf, p, escquote(0, s, '"') + 2);
	if (p) {
		*p++ = '"';
		p += escquote(p, s, '"');
		*p++ = '"';
	}
	return p;
}

/* convenient function:
 * prints arguments in ..., with and without double quotation (equivalent to
 * "%w" in sqlite3_mprintf) alternatively.
 * usefult for constructing an SQL statement. */
char *bufcat2(char **buf, char *p, ...)
{
	va_list va;
	const char *arg;
	int quote = 0;

	va_start(va, p);
	while ((arg = va_arg(va, const char *))) {
		p = (quote) ? bufcatQ(buf, p, arg) : bufcat(buf, p, arg);
		quote = !quote;
	}
	va_end(va);
	return p;
}
