#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "device-driver.h"

#include <QMainWindow>
#include <QMouseEvent>
#include <QThread>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
namespace Ui { class LoadWidget; }
namespace Ui { class ProcessWidget; }
namespace Ui { class AboutWidget; }
namespace Ui { class InfoWidget; }
namespace Ui { class TerminalWidget; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(QWidget *parent = nullptr);
	~MainWindow();

private:
	Ui::MainWindow *ui;
	Ui::LoadWidget *ui_loading;
	Ui::ProcessWidget *ui_process;
	Ui::AboutWidget *ui_about;
	Ui::InfoWidget *ui_info;
	Ui::TerminalWidget *ui_terminal;

	QWidget* loading;
	QWidget* process;
	QWidget* about;
	QWidget* info;
	QWidget* terminal;

	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);

	void ShowInitial();
	void ShowInfo(const QString&);
	void ShowLoading(const QString&);
	void ShowAbout();
	void ShowProcess();
	void WriteValuesToWindow();
	void ReadValuesFromControls();

	QString FormatSeconds(unsigned long);

	int m_nMouseClick_X_Coordinate;
	int m_nMouseClick_Y_Coordinate;

	void EnableButtons(bool);

public slots:
	void RefreshWindow();
	void CloseButton();
	void ConnectButton();
	void AboutButton();
	void SingleCycleButton();
	void WriteParametersButton();
	void WriteCountersButton();
	void SwitchToAdminMode();
	void ShowTerminal();
	void Event(DeviceDriver::EventCode);
	void TerminalTrace(const QString&);

signals:
	void ReadCounters();
	void WriteCounters(const DeviceDriver::Counters);

	void ReadParameters();
	void WriteParameters(const DeviceDriver::Parameters);

	void LaunchSingleCycle();
	void FindDevice();
	void Trace(const QString&);

private:
	enum class State {
		Initial,
		Connect,
		Ready,
		ReadCounters,
		WriteCounters,
		ReadParameters,
		WriteParameters,
		LaunchSingleCycle
	};

	State current_state;
	DeviceDriver device_driver;
	QThread device_driver_thread;

	DeviceDriver::Counters tmp_counters;
	DeviceDriver::Parameters tmp_parameters;

	DeviceDriver::Counters local_counters;
	DeviceDriver::Parameters local_parameters;
	DeviceDriver::MeasuredCharacteristics local_characteristics;

	bool admin_mode;

};
#endif // MAINWINDOW_H
