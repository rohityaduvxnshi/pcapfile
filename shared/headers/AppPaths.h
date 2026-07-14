#ifndef APPPATHS_H
#define APPPATHS_H

#include <QString>

// Standard on-disk locations for the Universal Data Suite, created on demand.
// Both apps point their export dialogs and project auto-saves here so the files
// survive clean rebuilds (the build directory is wiped on rebuild). Everything
// lives under the user's Documents folder:
//   <Documents>/UniversalDataSuite/Output Files
//   <Documents>/UniversalDataSuite/Projects
namespace AppPaths
{
// <Documents>/UniversalDataSuite (created if missing).
QString suiteRootDir();

// <root>/Output Files — default starting folder for every export dialog.
QString outputFilesDir();

// <root>/Projects — where the live-mode and simulator auto-saves are written.
QString projectsDir();
}

#endif // APPPATHS_H
