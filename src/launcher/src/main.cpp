#include "mainDialog.h"
#include "controllerNavigation.h"
#include "launcherTheme.h"

#include <QApplication>

int main(int argc, char* argv[]) {
	QApplication a(argc, argv);
	LauncherTheme::Initialize(a);
	ControllerNavigation controller_navigation;

	MainDialog w;

	w.emit Start();

	w.show();

	return QApplication::exec();
}
