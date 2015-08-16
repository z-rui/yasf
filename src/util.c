#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include <iup.h>
#include "ulbuf.h"

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

char *bufcat1(char **buf, char *p, const char *s)
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
		p = (quote) ? bufcat1(buf, p, arg) : bufcat(buf, p, arg);
		quote = !quote;
	}
	va_end(va);
	return p;
}

int IupMessage2(const char *title, const char *msg, const char *type, const char *buttons)
{
	Ihandle *dlg;
	int rc;

	dlg = IupMessageDlg();
	IupSetAttribute(dlg, "DIALOGTYPE", type);
	IupSetAttribute(dlg, "TITLE", title);
	IupSetAttribute(dlg, "BUTTONS", buttons);
	IupSetAttribute(dlg, "VALUE", msg);
	IupPopup(dlg, IUP_CURRENT, IUP_CURRENT);
	rc = IupGetInt(dlg, "BUTTONRESPONSE");
	IupDestroy(dlg);

	return rc;
}
