#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ui_load.h"
#include "ui_process.h"
#include "ui_about.h"
#include "ui_info.h"
#include "ui_terminal.h"

#include <QStyle>
#include <QTimer>
#include <QMovie>
#include <QFormLayout>
#include <QDebug>
#include <QShortcut>
#include <QDesktopWidget>
#include <QFontDatabase>

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
	, ui_loading(new Ui::LoadWidget)
	, ui_process(new Ui::ProcessWidget)
	, ui_about(new Ui::AboutWidget)
	, ui_info(new Ui::InfoWidget)
	, ui_terminal(new Ui::TerminalWidget)
	, loading(new QWidget)
	, process(new QWidget)
	, about(new QWidget)
	, info(new QWidget)
	, terminal(new QWidget)
	, current_state(State::Initial)
	, device_driver()
	, device_driver_thread()
	, local_counters({})
	, local_parameters({})
	, local_characteristics({})
	, admin_mode(false)
{
	ui->setupUi(this);

	ui_loading->setupUi(loading);
	ui_process->setupUi(process);
	ui_about->setupUi(about);
	ui_info->setupUi(info);
	ui_terminal->setupUi(terminal);


	// Драйвер устройства
	connect(this, &MainWindow::FindDevice, &device_driver, &DeviceDriver::FindDevice);
	connect(this, &MainWindow::ReadCounters, &device_driver, &DeviceDriver::ReadCounters);
	connect(this, &MainWindow::WriteCounters, &device_driver, &DeviceDriver::WriteCounters);
	connect(this, &MainWindow::ReadParameters, &device_driver, &DeviceDriver::ReadParameters);
	connect(this, &MainWindow::WriteParameters, &device_driver, &DeviceDriver::WriteParameters);
	connect(this, &MainWindow::LaunchSingleCycle, &device_driver, &DeviceDriver::LaunchSingleCycle);

	connect(&device_driver, &DeviceDriver::Event, this, &MainWindow::Event);
	connect(&device_driver, &DeviceDriver::Trace, this, &MainWindow::TerminalTrace);

	device_driver.moveToThread(&device_driver_thread);
	device_driver_thread.start();

	// Графика
	connect(ui->button_close, &QPushButton::clicked, this , &MainWindow::CloseButton);
	connect(ui->button_connect, &QPushButton::clicked, this, &MainWindow::ConnectButton);
	connect(ui->button_about, &QPushButton::clicked, this, &MainWindow::AboutButton);
	connect(ui_process->button_single_cycle, &QPushButton::clicked, this, &MainWindow::SingleCycleButton);
	connect(ui_process->button_write_data, &QPushButton::clicked, this, &MainWindow::WriteParametersButton);
	connect(ui_process->button_write_counters, &QPushButton::clicked, this, &MainWindow::WriteCountersButton);
	connect(this, &MainWindow::Trace, this, &MainWindow::TerminalTrace);


	QMovie* movie = new QMovie(":/logo/load.gif");
	ui_loading->load_logo->setMovie(movie);


	QFormLayout* body = new QFormLayout(this);
	body->addWidget(loading);
	body->addWidget(process);
	body->addWidget(about);
	body->addWidget(info);

	ShowInitial();
	ui->body->setLayout(body);

	QShortcut* ctl = new QShortcut(QKeySequence("Ctrl+Alt+N"), this);
	connect(ctl, &QShortcut::activated, this, &MainWindow::SwitchToAdminMode);

	QShortcut* term = new QShortcut(QKeySequence("Ctrl+Alt+T"), this);
	connect(term, &QShortcut::activated, this, &MainWindow::ShowTerminal);

	QFontDatabase::addApplicationFont(":/text/AT_Avant.ttf");
	QFont font = QFont("AT Avant");
	QApplication::setFont(font);
	this->update();
}

MainWindow::~MainWindow()
{
	device_driver_thread.quit();
	device_driver_thread.wait();
	delete ui;
}

void MainWindow::mousePressEvent(QMouseEvent* event)
{
	m_nMouseClick_X_Coordinate = event->x();
	m_nMouseClick_Y_Coordinate = event->y();
}

void MainWindow::mouseMoveEvent(QMouseEvent* event)
{
	move(event->globalX()-m_nMouseClick_X_Coordinate,event->globalY()-m_nMouseClick_Y_Coordinate);
}

void MainWindow::ShowInitial()
{
	EnableButtons(true);

	ui->button_connect->setText("Подключить устройство");
	ui->button_about->setText("О программе");

	about->hide();
	process->hide();
	loading->hide();
	info->hide();

	ui_loading->load_logo->movie()->stop();
}

void MainWindow::ShowInfo(const QString& text)
{
	EnableButtons(false);

	ui_info->info_text->setText(text);
	ui->button_connect->setText("Обновить данные");
	ui->button_about->setText("О программе");

	about->hide();
	process->hide();
	loading->hide();
	info->show();

	ui_loading->load_logo->movie()->stop();

	QTimer::singleShot(3000, this, &MainWindow::RefreshWindow);
}

void MainWindow::ShowLoading(const QString& text)
{
	EnableButtons(false);

	ui_loading->load_text->setText(text);
	ui->button_connect->setText("Обновить данные");
	ui->button_about->setText("О программе");

	about->hide();
	process->hide();
	info->hide();
	loading->show();

	if (ui_loading->load_logo->movie())
	ui_loading->load_logo->movie()->start();
}

void MainWindow::ShowAbout()
{
	EnableButtons(true);

	if (current_state == State::Ready) {
		ui->button_connect->setText("Обновить данные");
		ui->button_about->setText("Назад");
	} else {
		ui->button_about->setText("О программе");
		ui->button_connect->setText("Подключить устройство");
	}

	process->hide();
	info->hide();
	loading->hide();
	about->show();

	if (ui_loading->load_logo->movie()->state() != QMovie::Running) {
		ui_loading->load_logo->movie()->stop();
	}
}

void MainWindow::ShowProcess()
{
	WriteValuesToWindow();
	EnableButtons(true);

	ui->button_connect->setText("Обновить данные");
	ui->button_about->setText("О программе");

	info->hide();
	loading->hide();
	about->hide();
	process->show();
	ui_process->edit_counters_groupbox->setVisible(admin_mode);

	ui_loading->load_logo->movie()->stop();

	QRect rect = frameGeometry();
	rect.moveCenter(QDesktopWidget().availableGeometry().center());
	move(rect.topLeft());
}

void MainWindow::ShowTerminal()
{
	terminal->show();
}

void MainWindow::WriteValuesToWindow()
{
	ui_process->cycles_label->setText(QString::number(local_counters.cycles));
	ui_process->cycles_edit->setValue(local_counters.cycles);

	ui_process->time_label->setText(FormatSeconds(local_counters.time));
	ui_process->time_edit->setValue(local_counters.time);

	ui_process->cpm_label->setText(QString::number(local_parameters.cpm) + " мА");
	ui_process->cpm_edit->setValue(local_parameters.cpm);

	ui_process->tp_label->setText(QString::number(local_parameters.tp));
	ui_process->tp_edit->setValue(local_parameters.tp);

	ui_process->tbc_label->setText(QString::number(local_parameters.tbc));
	ui_process->tbc_edit->setValue(local_parameters.tbc);;

	ui_process->tbtp_label->setText(QString::number(local_parameters.tbtp));
	ui_process->tbtp_edit->setValue(local_parameters.tbtp);

	bool ct = local_parameters.ct;
	ui_process->ct_label->setText(ct ? "программно" : "аппаратно");
	ui_process->ct_edit->setChecked(!ct);

	ui_process->tw_label->setText(QString::number(local_parameters.tw));
	ui_process->tw_edit->setValue(local_parameters.tw);
}

void MainWindow::ReadValuesFromControls()
{
	tmp_counters.cycles = ui_process->cycles_edit->value();
	tmp_counters.time = ui_process->time_edit->value();

	tmp_parameters.cpm = ui_process->cpm_edit->value();
	tmp_parameters.tp = ui_process->tp_edit->value();
	tmp_parameters.tbc = ui_process->tbc_edit->value();
	tmp_parameters.tbtp = ui_process->tbtp_edit->value();
	tmp_parameters.ct = (ui_process->ct_edit->isChecked() ? 0x00 : 0xff);
	tmp_parameters.tw = ui_process->tw_edit->value();
}

QString MainWindow::FormatSeconds(unsigned long input_seconds)
{
	const int hours_in_day = 24;
	const int mins_in_hour = 60;
	const int secs_to_min = 60;

	long seconds = input_seconds % secs_to_min;
	long minutes = input_seconds / secs_to_min % mins_in_hour;
	long hours = input_seconds / secs_to_min / mins_in_hour % hours_in_day;
	long days = input_seconds / secs_to_min / mins_in_hour / hours_in_day;

	QString result =
			(days ? QString::number(days) + "д " : QString(""))
			+ (hours ? QString::number(hours) + "ч " : QString(""))
			+ (minutes ? QString::number(minutes) + "м " : QString(""))
			+ QString::number(seconds) + "с";
	return result;
}

void MainWindow::EnableButtons(bool value)
{
	ui->button_connect->setEnabled(value);
	ui->button_about->setEnabled(value);
}

void MainWindow::RefreshWindow()
{
	if (current_state == State::Initial) {
		ShowInitial();
	} else if (current_state == State::Ready) {
		ShowProcess();
	} else if (current_state == State::ReadCounters) {
		ShowLoading("Чтение счётчиков...");
		emit ReadCounters();
	}
}

void MainWindow::CloseButton()
{
	QCoreApplication::quit();
}

void MainWindow::ConnectButton()
{
	if (!device_driver.IsConnected() || current_state == State::Initial) {
		ShowLoading("Поиск устройства...");
		current_state = State::Connect;
		emit FindDevice();
	}

	if (current_state == State::Ready) {
		ShowLoading("Чтение счётчиков...");
		current_state = State::ReadCounters;
		emit ReadCounters();
	}
}

void MainWindow::AboutButton()
{
	if (about->isVisible()) {
		if (current_state == State::Ready) {
			ShowProcess();
		}
	} else {
		ShowAbout();
	}
}

void MainWindow::SingleCycleButton()
{
	ShowLoading("Однократный пуск цикла...");

	current_state = State::LaunchSingleCycle;
	emit LaunchSingleCycle();
}

void MainWindow::WriteParametersButton()
{
	ReadValuesFromControls();
	ShowLoading("Передача параметров...");
	current_state = State::WriteParameters;
	emit WriteParameters(tmp_parameters);
}

void MainWindow::WriteCountersButton()
{
	ReadValuesFromControls();
	ShowLoading("Передача счетчиков...");
	current_state = State::WriteCounters;
	emit WriteCounters(tmp_counters);
}

void MainWindow::SwitchToAdminMode()
{
	admin_mode = !admin_mode;
	RefreshWindow();
}

void MainWindow::Event(DeviceDriver::EventCode event)
{
	static const unsigned int kMaxRetryReadNumber = 2;
	static unsigned int retry_read_number = 0;
	QThread::msleep(200);

	if (event == DeviceDriver::EventCode::DeviceDisconnected) {
		current_state = State::Initial;
		ShowInfo("Подключение прервано :(");
		retry_read_number = 0;
		return;
	}

	switch (current_state) {
	case State::Initial: break;


	case State::Connect: {
		if (event == DeviceDriver::EventCode::DeviceFound)
		{
			retry_read_number = 0;
			current_state = State::ReadCounters;
			ShowLoading("Чтение счётчиков...");
			emit ReadCounters();
		}
		else if (event == DeviceDriver::EventCode::DeviceNotFound)
		{
			if (retry_read_number < kMaxRetryReadNumber) {
				++retry_read_number;
				emit Trace(QString("Retry connect №") + QString::number(retry_read_number));
				ShowLoading("Попытка переподключения...");
				current_state = State::Connect;
				emit FindDevice();
			} else {
				retry_read_number = 0;
				current_state = State::Initial;
				ShowInfo("Устройство не найдено :(");
				emit Trace("EventCode::DeviceNotFound\n");
			}
		}
	}
		break;


	case State::ReadCounters: {
		if (event == DeviceDriver::EventCode::ReadCountersSuccess)
		{
			retry_read_number = 0;
			local_counters = device_driver.GetCounters();
			current_state = State::ReadParameters;
			ShowLoading("Чтение параметров...");
			emit ReadParameters();
		}
		else if (event == DeviceDriver::EventCode::ReadCountersError)
		{
			if (retry_read_number < kMaxRetryReadNumber) {
				++retry_read_number;
				emit Trace(QString("Retry connect №") + QString::number(retry_read_number));
				ShowLoading("Попытка переподключения...");
				current_state = State::Connect;
				emit FindDevice();
			} else {
				retry_read_number = 0;
				current_state = State::Initial;
				ShowInfo("Ошибка чтения значений счётчиков :(");
				emit Trace("EventCode::ReadCountersError\n");
			}
		}
	}
		break;


	case State::ReadParameters: {
		if (event == DeviceDriver::EventCode::ReadParametersSuccess)
		{
			retry_read_number = 0;
			local_parameters = device_driver.GetParameters();
			current_state = State::Ready;
			ShowProcess();
		}
		else if (event == DeviceDriver::EventCode::ReadParametersError)
		{
			if (retry_read_number < kMaxRetryReadNumber) {
				++retry_read_number;
				emit Trace(QString("Retry connect №") + QString::number(retry_read_number));
				ShowLoading("Попытка переподключения...");
				current_state = State::Connect;
				emit FindDevice();
			} else {
				retry_read_number = 0;
				current_state = State::Initial;
				ShowInfo("Ошибка чтения значений параметров :(");
				emit Trace("EventCode::ReadParametersError\n");
			}
		}
	}
		break;
	case State::WriteCounters: {
		if (event == DeviceDriver::EventCode::WriteCountersSuccess)
		{
			current_state = State::ReadCounters;
			ShowInfo("Успешно!");
		}
		else if (event == DeviceDriver::EventCode::WriteCountersError)
		{
			current_state = State::Ready;
			ShowInfo("Ошибка передачи счётчиков :(");
			emit Trace("EventCode::WriteCountersError\n");
		}
	}
		break;
	case State::WriteParameters:
	{
			if (event == DeviceDriver::EventCode::WriteParametersSuccess)
			{
				current_state = State::ReadCounters;
				ShowInfo("Успешно!");
			}
			else if (event == DeviceDriver::EventCode::WriteParametersError)
			{
				current_state = State::Ready;
				ShowInfo("Ошибка передачи параметров :(");
				emit Trace("EventCode::WriteCountersError\n");
			}
		}
		break;
	case State::LaunchSingleCycle: {
		if (event == DeviceDriver::EventCode::LaunchSingleCycleSuccess)
		{
			local_characteristics = device_driver.GetCharacteristics();
			current_state = State::Ready;
			QString curr_str = QString::number(local_characteristics.curr * 0.001) + " А";
			QString vlt_str = QString::number(local_characteristics.vlt * 0.01) + " В";
			QString message = QString("Успешно.<br>Ток насоса = ")
					+ curr_str
					+ "<br>Напряжение питания = "
					+ vlt_str;
			ShowInfo(message);

			ui_process->vlt_label->setText(vlt_str);
			ui_process->curr_label->setText(curr_str);
		}
		else if (event == DeviceDriver::EventCode::LaunchSingleCycleError)
		{
			current_state = State::Ready;
			ShowInfo("Ошибка однократного запуска цикла :(");
			emit Trace("EventCode::LaunchSingleCycleError\n");
		}
	}
		break;
	case State::Ready: break;
	}
}


#include <chrono>

void MainWindow::TerminalTrace(const QString & str)
{
	static auto time_point = std::chrono::high_resolution_clock::now();
	auto trace_time = std::chrono::high_resolution_clock::now();
	auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(trace_time - time_point).count();
	ui_terminal->terminal->append(QString::number(delta) + "ms : " + str);
	time_point = trace_time;
}
