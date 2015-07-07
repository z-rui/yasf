#include <iup.h>
#include <assert.h>
#include "yasf.h"

int IupMain(int argc, char *argv[])
{
	int rc;
	Ihandle *dlg;

	db_init();

	dlg = IupGetHandle("dlg_main");
	rc = IupShow(dlg);
	assert(rc == IUP_NOERROR);

	rc = IupMainLoop();
	db_finalize();

	return rc;
}
