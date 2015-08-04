#include <iup.h>
#include <iupcontrols.h>
#include <iup_scintilla.h>

#include "regcb.h"

int main(int argc, char *argv[])
{
	int rc;
	extern int IupMain(int argc, char *argv[]);
	extern void led_load(void);

	IupOpen(&argc, &argv);
	IupControlsOpen();
	IupScintillaOpen();

	/* MBCS support is broken, always use UTF8 */
	IupSetGlobal("UTF8MODE", "yes");
	IupSetGlobal("UTF8MODE_FILE", "yes");

#include "regcb.c"

	led_load();
	rc = IupMain(argc, argv);

	/* finalize */
	IupControlsClose();
	IupClose();

	return rc;
}
