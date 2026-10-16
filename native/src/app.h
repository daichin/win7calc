/* GTK3 front end: window, menus, mouse and keyboard handling (port of win7calc/app.py). */
#ifndef WIN7CALC_APP_H
#define WIN7CALC_APP_H

/* Parse the command line, open the window and run the main loop; returns the exit status.
 * Hidden test hook:  win7calc --winshot OUT.png SCALE [STEP...]  (see tools/winshot.py steps). */
int app_main(int argc, char **argv);

/* Headless: win7calc --snapshot OUT.png MODE [CMD...]  (port of tools/snapshot.py). */
int snapshot_main(int argc, char **argv);

#endif
