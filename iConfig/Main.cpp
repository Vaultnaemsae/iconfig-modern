/*
;iConfig source code and documentation is released under a GPLv3 license. 
;
; A copy is available from the Open Source Initiative site at:
;	https://opensource.org/licenses/gpl-3.0.html
*/

#include "MainWindow.h"
#include "Version.h"

#include <QApplication>
#include <QIcon>
#include <QOperatingSystemVersion>
#include <QProcess>
#include <QSettings>
#include "QtSingleApplicationWrapper.h"

#ifdef Q_OS_MAC
#include "MacAppearance.h"
#endif

int main(int argc, char *argv[]) {

  int exitCode = 0;
  QString executablePath;
  QStringList arguments;

  {

#ifdef Q_OS_MAC
    if (QOperatingSystemVersion::current() >
        QOperatingSystemVersion(QOperatingSystemVersion::MacOS, 10, 8)) {
      // fix Mac OS X 10.9 (mavericks) font issue
      // https://bugreports.qt-project.org/browse/QTBUG-32789
      QFont::insertSubstitution(".Lucida Grande UI", "Lucida Grande");
    }
#endif

    QtSingleApplicationWrapper instance("com.vaultnaemsae.iconfig-modern",
                                        argc, argv);
    QThread::currentThread()->setPriority(QThread::LowPriority);
    if (instance.sendMessage("Application already running")) {
      return 0;
    }

    QCoreApplication::setOrganizationName("Vaultnaemsae");
    QCoreApplication::setOrganizationDomain("vaultnaemsae.com");
    QCoreApplication::setApplicationName("iConfig Modern");
    //QCoreApplication::setApplicationVersion("4.1.3");
    //QCoreApplication::setApplicationVersion("4.2.0");
    //bugfxing: version change to "4.2.1"
    //--zx,2016-06-08
    //QCoreApplication::setApplicationVersion("4.2.1");
    //QCoreApplication::setApplicationVersion("4.2.3");
    //--zx-03-23
    //QCoreApplication::setApplicationVersion("4.2.4");
    //QCoreApplication::setApplicationVersion("4.2.5"); //zx, 2017-04-26
    //QCoreApplication::setApplicationVersion("4.2.6"); //zx, 2017-06-22
    QCoreApplication::setApplicationVersion(ICONFIG_VERSION);
    instance.setWindowIcon(QIcon(":/Icon/AppIcon.png"));

#ifdef Q_OS_MAC
    QSettings appearanceSettings;
    appearanceSettings.beginGroup("appearance");
    applyMacAppearance(macAppearanceFromString(
        appearanceSettings.value("mode", "light").toString()));
    appearanceSettings.endGroup();
#endif

    executablePath = QCoreApplication::applicationFilePath();
    arguments = QCoreApplication::arguments();
    if (!arguments.isEmpty()) {
      arguments.removeFirst();
    }

    MainWindow w;
    w.show();

    exitCode = instance.exec();
  }

#ifdef Q_OS_MAC
  if (exitCode == kMacAppearanceRestartExitCode) {
    QProcess::startDetached(executablePath, arguments);
    return 0;
  }
#endif

  return exitCode;
}
