#ifndef DEVICEDRIVER_H
#define DEVICEDRIVER_H

#include <QSerialPort>
#include <QObject>
#include <QMutex>

class DeviceDriver : public QObject
{
    Q_OBJECT

public:

	enum class EventCode {
		ReadCountersSuccess,
		ReadCountersError,

		WriteCountersSuccess,
		WriteCountersError,

		ReadParametersSuccess,
		ReadParametersError,

		WriteParametersSuccess,
		WriteParametersError,

		LaunchSingleCycleSuccess,
		LaunchSingleCycleError,

		DeviceNotFound,
		DeviceFound,
		DeviceDisconnected
    };

    // Счетчики
    struct Counters {
        uint32_t time; // общее время работы (с)
        uint32_t cycles; // общее количество циклов
		static QByteArray Serialize(const Counters&);
		static Counters Deserialize(const QByteArray&);
    };

    // Параметры
    struct Parameters {
        uint16_t cpm; // граница тока мотора (мА)
        uint16_t tp; // время теста насоса [0 .. 10 000](мс)
        uint16_t tbc; // время паузы между циклами [0 .. 60 000](мс)
        uint16_t tbtp; // время между тестами насоса [0 .. 28 800](c)
        uint8_t ct; // время цикла установлена [программно (0xFF) / от внутреннего резистора (0x00)]
        uint16_t tw; // время цикла, если программно [40 .. 600](мс)
		static QByteArray Serialize(const Parameters&);
		static Parameters Deserialize(const QByteArray&);
    };

    // Измеренные характеристики
    struct MeasuredCharacteristics {
        uint16_t vlt; // напряжение питания платы цмр = 0.01 (В)
        uint16_t curr; // ток насоса во время цикла цмр = 0.01 (А)
		static MeasuredCharacteristics Deserialize(const QByteArray&);
     };

public:
    explicit DeviceDriver(QObject *parent = nullptr);
	~DeviceDriver();

	Counters GetCounters();
	Parameters GetParameters();
	MeasuredCharacteristics GetCharacteristics();
	bool IsConnected();

public slots:
	void FindDevice();

    void ReadCounters();
    void WriteCounters(const Counters);

    void ReadParameters();
    void WriteParameters(const Parameters);

    void LaunchSingleCycle();

	void HandleError(QSerialPort::SerialPortError error);
signals:
	void Event(EventCode);
	void Trace(const QString&);

private:
    bool _connected;
	Counters _counters;
	Parameters _parameters;
	MeasuredCharacteristics _characteristics;
	QMutex _data_mutex;

	QSerialPort* _serial_port;

private:
	void CloseSerialPort();
	bool CheckSerialPort(const QSerialPortInfo&);

	bool WaitReadyRead();
	QByteArray CreatePingMessage() const;
	QByteArray CreateReadCountersMessage() const;
	QByteArray CreateReadParametersMessage() const;
	QByteArray CreateSingleCycleMessage() const;

	QByteArray CreateWriteCountersMessage(const Counters &) const;
	QByteArray CreateWriteParametersMessage(const Parameters&) const;

	void AppendCrc(QByteArray&) const;
	bool CheckCrc(const QByteArray&) const;
	QByteArray CalculateCrc(const QByteArray&) const;
	QByteArray ExtractBody(const QByteArray&) const;
	QByteArray ExtractData(const QByteArray&) const;
	QByteArray ExtractCrc(const QByteArray&) const;
};

Q_DECLARE_METATYPE(DeviceDriver::EventCode)
Q_DECLARE_METATYPE(DeviceDriver::Counters)
Q_DECLARE_METATYPE(DeviceDriver::Parameters)

#endif // DEVICEDRIVER_H
