#include <ntqapplication.h>
#include <ntqstring.h>
#include "mainwindow.h"
#include <stdio.h>

int main(int argc, char **argv)
{
    TQApplication app(argc, argv);

    bool cmdline_execute = false;
    bool cmdline_stayopen = false;
    bool cmdline_import = false;
    TQString argv_session = "";
    TQString argv_filename = "";

    int i = 1;
    while (i < argc && argv[i] != NULL) {
        TQString arg = argv[i];
        if (arg.startsWith("-")) {
            if (arg.contains("e")) cmdline_execute = true;
            if (arg.contains("s")) cmdline_stayopen = true;
            if (arg.contains("i")) cmdline_import = true;
        } else {
            if (cmdline_import) {
                argv_filename = arg;
            } else {
                argv_session = arg;
            }
        }
        i++;
    }

    if (cmdline_execute && cmdline_import) {
        printf("Error: conflicting arguments\n");
        return 22;
    }

    MainWindow w;
    app.setMainWidget(&w);
    w.show();

    w.handleCommandLineArgs(cmdline_execute, cmdline_stayopen, cmdline_import, argv_session, argv_filename);

    return app.exec();
}
