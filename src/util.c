#include <stdlib.h>
#include <stddef.h>
#include <string.h>

struct buffer {
	size_t size, len;
	char bytes[1];
};

#define BUFFER(buf) ((struct buffer *)((buf) - offsetof(struct buffer, bytes)))

char *bufnew(size_t size)
{
	struct buffer *buf;

	buf = malloc(sizeof (struct buffer) + size);
	if (buf) {
		buf->size = size;
		buf->len = 0;
		buf->bytes[0] = 0;
		return buf->bytes;
	}
	return 0;
}

char *bufext(char *buf, size_t n, char **tail)
{
	size_t len;

	if (!buf) return 0;
	len = BUFFER(buf)->len + n;
	if (BUFFER(buf)->size < len) {
		size_t size = BUFFER(buf)->size;
		struct buffer *buffer;

		do
			size *= 2;
		while (size < len);
		if ((buffer = realloc(BUFFER(buf), sizeof (struct buffer) + size))) {
			buffer->size = size;
			buf = buffer->bytes;
		} else {	/* OOM */
			free(BUFFER(buf));
			return buf = 0;
		}
	}
	*tail = buf + BUFFER(buf)->len;
	BUFFER(buf)->len = len;
	buf[len] = 0;
	return buf;
}

char *bufcat(char *buf, const char *s)
{
	size_t slen;
	char *p;

	if (!buf) return 0;
	slen = strlen(s);
	if ((buf = bufext(buf, slen, &p)))
		memcpy(p, s, slen);
	return buf;
}

void buffree(char *buf)
{
	if (buf)
		free(BUFFER(buf));
}

size_t buflen(char *buf)
{
	return (buf) ? BUFFER(buf)->len : 0;
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
