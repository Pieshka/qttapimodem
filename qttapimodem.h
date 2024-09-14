/*
 * SPDX-FileCopyrightText: 2024 Pieszka
 * SPDX-License-Identifier: MIT
 */

#ifndef QTTAPIMODEM_H
#define QTTAPIMODEM_H

#include "qttapimodem_global.h"

#include <QIODevice>
#include <QMutex>
#include <QTimer>
#include <QSemaphore>
#include <QList>
#include <QEventLoop>
#include <QWinEventNotifier>
#include <QString>

#include <windows.h>
#include <tapi.h>

/* TAPI maximum supported API version */
static constexpr DWORD TAPI_SUPPORTED_API           = 0x00020002;
/* Friendly name for our application */
static constexpr const char *TAPI_FRIENDLYNAME      = "QtTapiModem";

/* Main TAPI Modem class
 *
 * Based on some implementations, mainly Microsoft's TAPICOMM example
 * and QExtSerialPort library.
 *
 * You need to initializeTAPI() first and then connectToNumber().
 * When you receive connected() signal or CallConnected call status,
 * you can start sending and receiving data.
 *
 * Beacuse of the nature of dial-up modems you can just get the connection
 * easily distrupted, so it's better to listen for call and line state changes.
 * Eventually the disconnected() signal will be emited.
 *
 * Errors are only for TAPI and IO operations - not what is happening on the line.
 * For this you need to listen for call and line state changes.
 *
 */
class QTM_EXPORT TAPIModem : public QIODevice
{
    Q_OBJECT

public:
    enum TAPIError {NoError = 0x00, InitError = 0x01, CommAquireError = 0x02, LineReplyError = 0x03, CallStatusAquireError = 0x04, CallDeallocationError = 0x05,
                    NoDeviceFoundError = 0x06, NegotiationError = 0x07, LineOpenError = 0x08, CallMakeError = 0x09, CommWriteError = 0x0A, LineDeallocationError = 0x0B,
                    CommReadError = 0x0B, OperationError = 0xFF};
    Q_FLAG(TAPIError)
    Q_DECLARE_FLAGS(TAPIErrors, TAPIError)

    enum TAPIState {Uninitialized = 0x00, Initialized = 0x01};
    Q_FLAG(TAPIState)
    Q_DECLARE_FLAGS(TAPIStates, TAPIState)

    enum CallState {CallDefaultState = 0x00, CallDialing = 0x01, CallBusy = 0x02, CallIdle = 0x03, CallCannotDial = 0x04, CallDisconnected = 0x05,
                    CallConnected = 0x06, CallUnknown = 0xFF};
    Q_FLAG(CallState)
    Q_DECLARE_FLAGS(CallStates, CallState)

    enum DisconnectReason {DisconnectDefaultState = 0x00, DisconnectByRemote = 0x01, DisconnectReject = 0x02, DisconnectPickup = 0x03, DisconnectForwarded = 0x04,
                           DisconnectBusy = 0x05, DisconnectNoAnswer = 0x06, DisconnectBadAddress = 0x07, DisconnectUnreachable = 0x08, DisconnectCongestion = 0x09,
                           DisconnectIncompatible = 0x0A, DisconnectUnavailable = 0x0B, DisconnectNoDialTone = 0x0C, DisconnectBlocked = 0x0D, DisconnectCancelled = 0x0E,
                           DisconnectDoNotDisturb = 0x0F, DisconnectNumberChanged = 0x10, DisconnectOutOfOrder = 0x11, DisconnectQOSUnavailable = 0x12,
                           DisconnectTemporaryFailure = 0x13, DisconnectedByFunction = 0xFE, DisconnectUnknown = 0xFF};
    Q_FLAG(DisconnectReason)
    Q_DECLARE_FLAGS(DisconnectReasons, DisconnectReason)

    enum LineState {LineClosed = 0x00, LineOpened = 0x01, LineDisconnected = 0x02, LineMaintenance = 0x03, LineOutOfService = 0x04, LineDeviceRemoved = 0x05,
                    LineReinitialization = 0x06, LineUnknown = 0xFF};
    Q_FLAG(LineState)
    Q_DECLARE_FLAGS(LineStates, LineState)

    TAPIModem(QObject *parent = 0);
    virtual ~TAPIModem();

    //bool open(QIODevice::OpenMode mode = QIODevice::ReadWrite); // We don't need open() to be honest. TAPI is more of a mix between QTcpSocket and QSerialPort
    bool initializeTAPI();
    bool initializeTAPI(QString appName);
    void connectToNumber(quint32 modemId, QString destNumber);
    void connectToNumber();
    void endConnection();

    bool isSequential() const { return true; }
    qint64 bytesAvailable() const;

    bool waitForReadyRead(int msecs = 30000);
    bool waitForConnected(int msecs = 30000);
    bool waitForDisconnected(int msecs = 30000);

    void setDeviceId(quint32 deviceId) { dwDeviceId = (DWORD)deviceId; }
    void setFriendlyName(QString name) { friendlyName = name; }
    void setDestinationNumber(QString number) { destinationNumber = number; }

    TAPIError error() { return errFlag; }
    void clearError() { errFlag = NoError; }

    TAPIState tapiState() { return tapiStateFlag; }
    CallState callState() { return callStateFlag; }
    DisconnectReason disconnectReason() { return disconnectFlag; }
    LineState lineState() { return lineStateFlag; }

private:
    /* General variables */
    QString friendlyName = QString(TAPI_FRIENDLYNAME);
    TAPIError errFlag = NoError;
    TAPIState tapiStateFlag = Uninitialized;
    CallState callStateFlag = CallDefaultState;
    DisconnectReason disconnectFlag = DisconnectDefaultState;
    LineState lineStateFlag = LineClosed;

    /* Initialization variables */
    HLINEAPP hLineApp = 0;
    DWORD dwDeviceNumber;

    QMutex lineAppMutex;
    QWinEventNotifier * tapiEventNotifier = 0;

    /* Call specific variables */
    HCALL hcCurrentCall = 0;
    QString destinationNumber;

    QMutex callMutex;

    /* Device/Line specific variables */
    HLINE hlDevice = 0;
    DWORD dwDeviceId = 0;

    QMutex lineMutex;

    /* Data communication specific variables */
    HANDLE hCommFile = INVALID_HANDLE_VALUE;
    OVERLAPPED overlap;
    DWORD receivedEventMask;
    QList<OVERLAPPED *> pendingOverlappedWrites;

    QWinEventNotifier * commIOEventNotifier = 0;
    QMutex commMutex;

    QSemaphore bufferSem;
    QByteArray modemReadBuffer;

public slots:
    void close();

private slots:
    void on_TAPIevent();
    void on_COMevent();

    void com_readReady();

private:
    void initializeCommPort();

    void deinitializeTAPI();
    void shutdownTAPI();
    void hangupCall();
    void deinitializeCommPort();

    qint64 comBytesAvailable();

protected:
    qint64 readData(char *data, qint64 maxlen);
    qint64 writeData(const char *data, qint64 len);

signals:
    void errorOccurred(TAPIError);

    void tapiStateChanged(TAPIState);
    void callStateChanged(CallState);
    void lineStateChanged(LineState);

    void connected();
    void disconnected();

    /* Private signals */
    void lineReplyOccured(QPrivateSignal, LONG request, LONG reply);
};

/* Info class for modems
 * Similar to QSerialPortInfo class
 *
 * We are getting only modem id in TAPI ecosystem
 * and modem name. We could get additional info, but
 * to be honest modem driver were always a mess.
 * Sometimes manufacturer or model fields were empty.
 * It's better to present a modem to user only by it's name.
 *
 */

class QTM_EXPORT TAPIModemInfo
{
public:
    static QList<TAPIModemInfo> availableModems();
    qint32 deviceId() { return devId; }
    QString modemName() { return name; }

private:
    qint32 devId;
    QString name;
};

/* Simple class for building dialable numbers
 *
 * Using it, we can quickly build a number in
 * canonical form or add some pauses (good for
 * DISA dialing).
 *
 * Typically one period (',') is one second of a pause,
 * but it may be different on different modems. If you
 * are using particular device model, you can tweak the code.
 * Otherwise it is better to leave everything as is and let
 * the user to put pauses themself.
 *
 */

class QTM_EXPORT DialableNumberBuilder
{
public:
    DialableNumberBuilder();

    DialableNumberBuilder& AddCountryCode(int countryCode);
    DialableNumberBuilder& AddAreaCode(int areaCode);
    DialableNumberBuilder& AddNumber(QString number);
    DialableNumberBuilder& AddPause(int durationSec = 1);

    QString Build();

private:
    QString _dialableNumber;
};

#endif // QTTAPIMODEM_H
