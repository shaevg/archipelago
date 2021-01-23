#include "device-driver.h"
#include <QThread>
#include <QMutexLocker>
#include <QSerialPortInfo>
#include <QDebug>

namespace Codes {
const QByteArray kMasterSlave = "@";
const QByteArray kSlaveMaster = "$";
const QByteArray kCRLF = "\r\n";
const QByteArray kPing = "55";

const QByteArray kReadCounters = "20";
const QByteArray kReadParameters = "30";
const QByteArray kSingleCycle = "40";

const QByteArray kWriteCounters = "2F";
const QByteArray kWriteParameters = "3F";
}

DeviceDriver::DeviceDriver(QObject *parent)
    : QObject(parent)
    , _connected(false)
	, _counters({})
	, _parameters({})
	, _characteristics({})
	, _serial_port(nullptr)
{
	qRegisterMetaType<EventCode>("EventCode");
	qRegisterMetaType<Counters>("Counters");
	qRegisterMetaType<Parameters>("Parameters");
}

DeviceDriver::~DeviceDriver()
{
	CloseSerialPort();
	_connected = false;
}

DeviceDriver::Counters DeviceDriver::GetCounters()
{
	QMutexLocker locker(&_data_mutex);
	return _counters;
}

DeviceDriver::Parameters DeviceDriver::GetParameters()
{
	QMutexLocker locker(&_data_mutex);
	return _parameters;
}

DeviceDriver::MeasuredCharacteristics DeviceDriver::GetCharacteristics()
{
	QMutexLocker locker(&_data_mutex);
	return _characteristics;
}

bool DeviceDriver::IsConnected()
{
	return _connected;
}

void DeviceDriver::FindDevice()
{
	const auto available_ports = QSerialPortInfo::availablePorts();

	emit Trace("Available devices:");
	for (const auto& port : available_ports) {
		emit Trace(port.portName() + " " + port.description());
	}

	for (const auto& port : available_ports) {

		if (CheckSerialPort(port)) {
			_connected = true;
			break;
		}
	}

	if (_connected) {
		emit Event(EventCode::DeviceFound);
	} else {
		emit Event(EventCode::DeviceNotFound);
	}
}

void DeviceDriver::ReadCounters()
{
	static const QByteArray kRequest = CreateReadCountersMessage();
	if (_connected)
	{
		emit Trace(QString("out > ") + kRequest);
		_serial_port->write(kRequest);

		if (WaitReadyRead())
		{
			auto raw = _serial_port->readAll();
			emit Trace(QString("in   < ") + raw);
			if (raw.startsWith(Codes::kSlaveMaster + Codes::kReadCounters)
					&& CheckCrc(raw))
			{
				_counters = Counters::Deserialize(ExtractData(raw));
				emit Event(EventCode::ReadCountersSuccess);
				return;
			}
		}
    }

	CloseSerialPort();
	emit Event(EventCode::ReadCountersError);
}

void DeviceDriver::WriteCounters(const DeviceDriver::Counters counters)
{
	const QByteArray kRequest = CreateWriteCountersMessage(counters);

	if (_connected)
	{
		emit Trace(QString("out > ") + kRequest);
		_serial_port->write(kRequest);

		if (WaitReadyRead())
		{
			auto raw = _serial_port->readAll();
			emit Trace(QString("in   < ") + raw);
			if (raw.startsWith(Codes::kSlaveMaster + Codes::kWriteCounters)
					&& CheckCrc(raw))
			{
				emit Event(EventCode::WriteCountersSuccess);
				return;
			}
		}
	}

	CloseSerialPort();
	emit Event(EventCode::WriteCountersError);
}

void DeviceDriver::ReadParameters()
{
	static const QByteArray kRequest = CreateReadParametersMessage();
	if (_connected)
	{
		emit Trace(QString("out > ") + kRequest);
		_serial_port->write(kRequest);

		if (WaitReadyRead())
		{
			auto raw = _serial_port->readAll();
			emit Trace(QString("in   < ") + raw);
			if (raw.startsWith(Codes::kSlaveMaster + Codes::kReadParameters)
					&& CheckCrc(raw))
			{
				_parameters = Parameters::Deserialize(ExtractData(raw));
				emit Event(EventCode::ReadParametersSuccess);
				return;
			}
		}
	}

	CloseSerialPort();
	emit Event(EventCode::ReadParametersError);
}

void DeviceDriver::WriteParameters(const DeviceDriver::Parameters parameters)
{
	const QByteArray kRequest = CreateWriteParametersMessage(parameters);

	if (_connected)
	{
		emit Trace(QString("out > ") + kRequest);
		_serial_port->write(kRequest);

		if (WaitReadyRead())
		{
			auto raw = _serial_port->readAll();
			emit Trace(QString("in   < ") + raw);
			if (raw.startsWith(Codes::kSlaveMaster + Codes::kWriteParameters)
					&& CheckCrc(raw))
			{
				emit Event(EventCode::WriteParametersSuccess);
				return;
			}
		}
	}

	CloseSerialPort();
	emit Event(EventCode::WriteParametersError);
}

void DeviceDriver::LaunchSingleCycle()
{
	static const QByteArray kRequest = CreateSingleCycleMessage();
	if (_connected)
	{
		emit Trace(QString("out > ") + kRequest);
		_serial_port->write(kRequest);

		if (WaitReadyRead())
		{
			auto raw = _serial_port->readAll();
			emit Trace(QString("in   < ") + raw);
			if (raw.startsWith(Codes::kSlaveMaster + Codes::kSingleCycle)
					&& CheckCrc(raw))
			{
				_characteristics = MeasuredCharacteristics::Deserialize(ExtractData(raw));
				emit Event(EventCode::LaunchSingleCycleSuccess);
				return;
			}
		}
	}

	CloseSerialPort();
	emit Event(EventCode::LaunchSingleCycleError);
}

void DeviceDriver::HandleError(QSerialPort::SerialPortError error)
{
	if (error != QSerialPort::NoError) {
		emit Trace(QString("serial-port error : ") + _serial_port->errorString());
		emit Event(EventCode::DeviceDisconnected);
		CloseSerialPort();
	}
}

void DeviceDriver::CloseSerialPort()
{
	_connected = false;
	if (_serial_port) {
		if (_serial_port->isOpen()) {
			_serial_port->close();
		}

		_serial_port->deleteLater();
		_serial_port = nullptr;
	}
}

bool DeviceDriver::CheckSerialPort(const QSerialPortInfo & info)
{
	const auto ping = CreatePingMessage();

	if (!_serial_port)
	{
		_serial_port = new QSerialPort(info, this);
		_serial_port->setBaudRate(QSerialPort::Baud115200);
		_serial_port->setDataBits(QSerialPort::Data8);
		_serial_port->setParity(QSerialPort::Parity::NoParity);
		_serial_port->setStopBits(QSerialPort::StopBits::OneStop);
		_serial_port->setFlowControl(QSerialPort::FlowControl::NoFlowControl);

		emit Trace(QString("Try open -> ") + info.portName());
		if (_serial_port->open(QIODevice::ReadWrite))
		{
			emit Trace(QString("out > ") + ping);
			_serial_port->write(ping);

			if (WaitReadyRead())
			{
				auto raw =_serial_port->readAll();
				emit Trace("in   < " + raw);
				if (raw.startsWith(Codes::kSlaveMaster + Codes::kPing))
				{
					connect(_serial_port,
							&QSerialPort::errorOccurred,
							this,
							&DeviceDriver::HandleError);
					return true;
					emit Trace("ok : " + info.portName());
				}
			}
		}
	}

	emit Trace("error : " + info.portName());
	CloseSerialPort();
	return false;
}

bool DeviceDriver::WaitReadyRead()
{
	if(_serial_port
			&& _serial_port->waitForBytesWritten(2000)
			&& _serial_port->waitForReadyRead(2000))
	{
		return true;
	}
	return false;
}

QByteArray DeviceDriver::CreatePingMessage() const
{
	return (Codes::kMasterSlave + Codes::kPing + Codes::kCRLF).toUpper();
}

QByteArray DeviceDriver::CreateReadCountersMessage() const
{
	auto result = Codes::kReadCounters;
	AppendCrc(result);
	return (Codes::kMasterSlave + result + Codes::kCRLF).toUpper();
}

QByteArray DeviceDriver::CreateReadParametersMessage() const
{
	auto result = Codes::kReadParameters;
	AppendCrc(result);
	return (Codes::kMasterSlave + result + Codes::kCRLF).toUpper();
}

QByteArray DeviceDriver::CreateSingleCycleMessage() const
{
	auto result = Codes::kSingleCycle;
	AppendCrc(result);
	return (Codes::kMasterSlave + result + Codes::kCRLF).toUpper();
}

QByteArray DeviceDriver::CreateWriteCountersMessage(const DeviceDriver::Counters & counters) const
{
	QByteArray result = Codes::kWriteCounters + Counters::Serialize(counters);
	AppendCrc(result);
	return (Codes::kMasterSlave + result + Codes::kCRLF).toUpper();
}

QByteArray DeviceDriver::CreateWriteParametersMessage(const DeviceDriver::Parameters & parameters) const
{
	QByteArray result = Codes::kWriteParameters + Parameters::Serialize(parameters);
	AppendCrc(result);
	return (Codes::kMasterSlave + result + Codes::kCRLF).toUpper();
}

unsigned char CRC8IN(unsigned char last_crc, unsigned char input_data)
{
	const unsigned char kPolynomial = 0x31;
	unsigned char crc = last_crc;
	crc ^= input_data;
	for(int i = 8; i; --i) {
		crc = (crc & 0x80)
				? ((crc << 1) ^ kPolynomial)
				: (crc << 1);
	}
	return crc;
}

void DeviceDriver::AppendCrc(QByteArray & data) const
{
	data.append(CalculateCrc(data));
}

QByteArray DeviceDriver::CalculateCrc(const QByteArray& data) const
{
	QByteArray tmp = QByteArray::fromHex(data);

	unsigned char last_crc = 0xFF;
	for (auto byte : tmp) {
		last_crc = CRC8IN(last_crc, static_cast<unsigned char>(byte));
	}

	QByteArray crc;
	crc = crc.append(last_crc).toHex();
	crc = crc.toUpper();
	return crc;
}

QByteArray DeviceDriver::ExtractBody(const QByteArray & data) const
{
	const unsigned int kCrcLength = 2;
	const unsigned int kMessageIndex = 1;
	const int kMessageLength = data.length()
			- kMessageIndex
			- Codes::kCRLF.length()
			- kCrcLength;

	QByteArray result;
	if (kMessageLength > 0) {
		result.append(data.mid(kMessageIndex, kMessageLength));
	}
	return result;
}

QByteArray DeviceDriver::ExtractData(const QByteArray & data) const
{
	const unsigned int kCrcLength = 2;
	const unsigned int kMessageIndex = 3;
	const int kMessageLength = data.length()
			- kMessageIndex
			- Codes::kCRLF.length()
			- kCrcLength;

	QByteArray result;
	if (kMessageLength > 0) {
		result.append(data.mid(kMessageIndex, kMessageLength));
	}
	return result;
}

QByteArray DeviceDriver::ExtractCrc(const QByteArray & data) const
{
	const unsigned int kCrcLength = 2;
	const unsigned int kMessageIndex = 1;
	const int kMessageLength = data.length()
			- kMessageIndex
			- Codes::kCRLF.length()
			- kCrcLength;
	const unsigned int kCrcIndex = kMessageIndex + kMessageLength;

	QByteArray result;

	if (kMessageLength > 0) {
		result.append(data.mid(kCrcIndex, kCrcLength));
	}
	return result;
}

bool DeviceDriver::CheckCrc(const QByteArray& data) const
{
	auto crc = ExtractCrc(data);
	auto tmp = ExtractBody(data);
	auto calc_crc = CalculateCrc(tmp);
	if (crc.length()
			&& tmp.length()
			&& calc_crc.length()
			&& calc_crc == crc)
	{
		return true;
	}
	return false;
}

template <class T>
QByteArray ValueToRaw(T value)
{
	const int kSize = sizeof (T);
	QByteArray result;

	for (int i = 0; i < kSize; ++i) {
		result.append(static_cast<unsigned char>(value & 0xff));
		value >>= 8;
	}
	return result.toHex();
}

QByteArray DeviceDriver::Counters::Serialize(const DeviceDriver::Counters & counters)
{
	QByteArray result;
	result.append(ValueToRaw<uint32_t>(counters.cycles));
	result.append(ValueToRaw<uint32_t>(counters.time));
	return result;
}

template<class T>
T ValueFromRaw(const QByteArray& raw) {
	uint32_t result = 0;
	const int kRawSize = sizeof (T);
	if (raw.length() >= kRawSize) {
		QByteArray tmp = raw.mid(0, kRawSize);
		std::reverse(tmp.begin(), tmp.end());
		for (int i = 0; i < kRawSize; ++i) {
			result <<= 8;
			result += static_cast<unsigned char>(tmp[i]);
		}
	}

	return result;
}

DeviceDriver::Counters DeviceDriver::Counters::Deserialize(const QByteArray & raw)
{
	const int kRawSize = 8;
	Counters result = {};
	QByteArray tmp = QByteArray::fromHex(raw);
	if (tmp.length() == kRawSize) {

		result.cycles = ValueFromRaw<uint32_t>(tmp);
		tmp.remove(0, sizeof (uint32_t));

		result.time = ValueFromRaw<uint32_t>(tmp);
	}
	return result;
}

QByteArray DeviceDriver::Parameters::Serialize(const DeviceDriver::Parameters & parameters)
{
	QByteArray result;
	result.append(ValueToRaw<uint16_t>(parameters.cpm));
	result.append(ValueToRaw<uint16_t>(parameters.tp));
	result.append(ValueToRaw<uint16_t>(parameters.tbc));
	result.append(ValueToRaw<uint16_t>(parameters.tbtp));
	result.append(ValueToRaw<uint8_t>(parameters.ct));
	result.append(ValueToRaw<uint16_t>(parameters.tw));
	return result;
}

DeviceDriver::Parameters DeviceDriver::Parameters::Deserialize(const QByteArray & raw)
{
	const int kRawSize = 11;
	Parameters result = {};
	QByteArray tmp = QByteArray::fromHex(raw);

	if (tmp.length() == kRawSize) {

		result.cpm = ValueFromRaw<uint16_t>(tmp);
		tmp.remove(0, sizeof (uint16_t));

		result.tp = ValueFromRaw<uint16_t>(tmp);
		tmp.remove(0, sizeof (uint16_t));

		result.tbc = ValueFromRaw<uint16_t>(tmp);
		tmp.remove(0, sizeof (uint16_t));

		result.tbtp = ValueFromRaw<uint16_t>(tmp);
		tmp.remove(0, sizeof (uint16_t));

		result.ct = ValueFromRaw<uint8_t>(tmp);
		tmp.remove(0, sizeof (uint8_t));

		result.tw = ValueFromRaw<uint16_t>(tmp);
	}
	return result;
}

DeviceDriver::MeasuredCharacteristics DeviceDriver::MeasuredCharacteristics::Deserialize(const QByteArray& raw)
{
	const int kRawSize = 4;
	MeasuredCharacteristics result = {};
	QByteArray tmp = QByteArray::fromHex(raw);

	if (tmp.length() == kRawSize) {

		result.vlt = ValueFromRaw<uint16_t>(tmp);
		tmp.remove(0, sizeof (uint16_t));

		result.curr = ValueFromRaw<uint16_t>(tmp);
	}
	return result;
}
