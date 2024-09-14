## Introduction
**QtTAPIModem** is a simple Qt library that lets you interface with dial-up modems *(yes, those squeaky boxes from the 90s)*, using Microsoft's **Telephony Application Programming Interface**. You may ask, *"But why would I ever need to interface with a modem in \<insert here current year>?"* That's a good question! I don't know either! But there are companies still making dial-up modems and other ancient devices like network hubs, so someone has an interest in using these. I can think of some use-cases like remotely controlling industrial or telephony-oriented devices (e.g., PBXes, military telecommunication equipment). If you just happen to need an easy way to talk with dial-up modems, and you don't want to use QSerialPort with the AT command set, you are in the right place!

> IMPORTANT! Because this library uses TAPI, which is a Windows-only feature, you can't use QtTAPIModem on any other platform!

> ALSO IMPORTANT! QtTAPIModem is **NOT** a Qt-oriented TAPI wrapper library, so it's very limited with what it can do. 

## Basic principles
The main class `TAPIModem` derives from `QIODevice` and behaves like a mixture of `QSerialPort` and `QAbstractSocket`. If you worked with socket-based classes (`QTcpSocket` for example), you know that connecting to a remote server requires you to provide an IP Address and a TCP Port. Here, you need to provide a destination number in the [correct form](https://learn.microsoft.com/pl-pl/windows/win32/tapi/address-ovr).  You also can't immediately read and write until the connection is established. The same principle works here - you need to wait for the call to be connected. But on the other hand, a phone line is just a very long serial cable, so many of the `QSerialPort` principles apply, and under the hood, we are operating on a normal serial port handle when reading and writing data.

The goal of this library is to give a seamless experience based on the rule that *if something breaks - disconnect*, so dealing with TAPI and its shenanigans is done on our part. You are provided with basic options like **TAPI initialization**, **call (connection) establishment** and **call (connection) hangup** and a very detailed feedback system which informs you about errors and state changes, but if your application is not TAPI-oriented but - for example - provides dial-up modem as one of the connection options, we also got you covered and connecting to two signals `connected` and `disconnected` gives you the ability to know when the connection is definitely established and ended without the need to interpret many of the TAPI states.

## Usage
The following example shows a very basic connection procedure:

```cpp
// This is a very simple example. If you need something more functional, 
// see the examples folder

// First, we need to instantiate the TAPIModem class
TAPIModem * modem = new TAPIModem(this);

// Then we need to initialize TAPI. To know if the initialization was
// successful, you can connect to the "tapiStateChanged" signal or
// you the value returned by the function.
//
// You must also provide a friendly name for the TAPI which will be used
// throughout the whole subsystem, for example, in other applications
if(!modem->initializeTAPI("My First TAPI Program"))
{
	qDebug() << "TAPI initialization error";
	return;
}

// We need to know what modems are installed in the system and what their IDs are
// To obtain this information, we will use the TAPIModemInfo helper class
QList<TAPIModemInfo> availableModems = TAPIModemInfo::availableModems();
// This loop is only to show available members of the TAPIModemInfo list
for(TAPIModemInfo info : availableModems)
	qDebug() << info.deviceId() << ". " << info.modemName();

// Let's assume we have installed one modem
// Now we need to dial a destination number of choice via the selected modem
// and that's it
modem->connectToNumber(availableModems[0].deviceId(), QString("123123123"));

// If the call gets established and modems get synchronized, the connected()
// signal will be emitted, and also the Call State will be set to CallConnected
// (and the callStateChanged signal will also be emitted).

// If the call gets terminated, the disconnected() signal will ALWAYS be emitted
// and the CallState, LineState (TAPI State if serious error 
// occurred) and DisconnectReason will also be changed accordingly.

//You can always end the call using endConnection()
modem->endConnection();
```

## States and Errors
QtTAPIModem provides a very wide range of feedback from TAPI, so you can provide your user with a pretty decent explanation of why the call got terminated, because, let's be honest - dial-up modems aren't the most stable type of connection, so random errors will show. Even TAPI is not a very good subsystem and can act quirky sometimes. To act accordingly, you may want to connect to the `errorOccurred`, `tapiStateChanged`, `callStateChanged` and `lineStateChanged` signals.

### Errors
Errors in QtTAPIModem **ONLY** describe issues with TAPI and I/O operations. What happens on the line and with the device is not signaled by errors. Keep that in mind.
| Constant | Description |
|----------|-------------|
| `TAPIModem::NoError` | No error occured |
| `TAPIModem::InitError` | An error occured when initializing TAPI |
| `TAPIModem::CommAquireError` | An error occured when aquiring COM port handle from TAPI |
| `TAPIModem::LineReplyError` | Some asynchronously invoked operation failed, but it shouldn't |
| `TAPIModem::CallStatusAquireError` | An error occured when aquiring status about the call |
| `TAPIModem::CallDeallocationError` | An error occured when trying to deallocate the call |
| `TAPIModem::NoDeviceFoundError` | Provided device ID was not found |
| `TAPIModem::NegotiationError` | An error occured when negotiation TAPI version with the modem |
| `TAPIModem::LineOpenError` | An error occured when opening the modem |
| `TAPIModem::CallMakeError` | An error occured when trying to make the call |
| `TAPIModem::CommWriteError` | An error occured when writing data to the modem |
| `TAPIModem::LineDeallocationError` | An error occured when trying to close the modem |
| `TAPIModem::CommReadError` | An error occured when trying to read data from the modem |
| `TAPIModem::OperationError` | A unknown and unhandled error occured | 

The library is not providing detailed information about each error because it will add unnecessary complexity, and if any of the TAPI-oriented errors occur, you can safely assume that TAPI got crazy, which on newer Windows OSes is not a rare sight.

### TAPIState
TAPI states provide information regarding the sole Telephony API.
| Constant | Description |
|----------|-------------|
| `TAPIModem::Uninitialized` | TAPI is not initialized. You must use `initializeTAPI()` |
| `TAPIModem::Initialized` | TAPI is initialized and is ready for establishing connection |

### LineState
Line states inform about the modem, but not the call!
| Constant | Description |
|----------|-------------|
| `TAPIModem::LineClosed` | Connection with the modem is closed. It is automatically established when using `connectToNumber()` |
| `TAPIModem::LineOpened` | Connection with the modem is opened. This means, everything between the device and TAPI is in good condition. This signal is emitted while executing `connectToNumber()` |
| `TAPIModem::LineDisconnected` | Connection with the modem is closed, but previously it was `LineOpened`. If connection was ended using `endConnection()` or got forcibly closed this state gets overwritten by `LineClosed` |
| `TAPIModem::LineMaintenance` | Some maintenance is being performed on the modem and TAPI cannot use it |
| `TAPIModem::LineOutOfService` | Phone line is out of service or got physically disconnected |
| `TAPIModem::LineDeviceRemoved` | Modem got removed from the system |
| `TAPIModem::LineReinitialization` | TAPI wanted from us to reinitialize. QtTAPIModem doesn't automatically reinitialize, so you must use `initializeTAPI()` and establish connection again |
| `TAPIModem::LineUnknown` | Unknown state of the line. Currently it's not used |

### CallState
Call states inform about the call between modems.
| Constant | Description |
|----------|-------------|
| `TAPIModem::CallDefaultState` | Default state of the call before dialing a number and after using `endConnection()` |
| `TAPIModem::CallDialing` | Modem is currently dialing or negotiating the connection (it's more like the modem is dialing but TAPI doesn't have a way to determine if modem is negotiating so TAPI never exits the dialing state until the negotiation ends) |
| `TAPIModem::CallBusy` | Callee is busy |
| `TAPIModem::CallIdle` | Call got in a idle state, which means it exists but only inside TAPI subsystem. On the phone line there is no call. Call will temporiarily enter Idle state when executing `endConnection()` as a step in terminating |
| `TAPIModem::CallCannotDial` | For some unknown reason we cannot dial the callee |
| `TAPIModem::CallDisconnected` | Call was established but then got disconnected. If connection was ended using `endConnection()` this state immediately changes into `CallDefaultState` |
| `TAPIModem::CallConnected` | Call is established and data flows between modems |
| `TAPIModem::CallUnknown` | Unknown state of the call. Currently it's not used |

### DisconnectReason
After the call gets disconnected, you can use `disconnectReason()` to get more detailed information about the disconnection reason. Remember that most of these reasons don't even apply to analog phone lines.
| Constant | Description |
|----------|-------------|
| `TAPIModem::DisconnectDefaultState` | Default state. Call didn't get disconnected even once |
| `TAPIModem::DisconnectByRemote` | Call was terminated by the remote party |
| `TAPIModem::DisconnectReject` | Call was rejected by the remote party |
| `TAPIModem::DisconnectPickup` | Call was picked up from somewhere else |
| `TAPIModem::DisconnectForwarded` | Call was forwarded |
| `TAPIModem::DisconnectBusy` | Remote party is busy |
| `TAPIModem::DisconnectNoAnswer` | Remote party doesn't answer |
| `TAPIModem::DisconnectBadAddress` | Destination address is invalid |
| `TAPIModem::DisconnectUnreachable` | Remote party is unreachable |
| `TAPIModem::DisconnectCongestion` | The network is congested |
| `TAPIModem::DisconnectIncompatible` | Remote party's equipment is incompatbile with requested call |
| `TAPIModem::DisconnectUnavailable` | Remote party is unavailable |
| `TAPIModem::DisconnectNoDialTone` | No dial tone was detected on the line |
| `TAPIModem::DisconnectBlocked` | Call was blocked by the remote party |
| `TAPIModem::DisconnectCancelled` | The call was cancelled |
| `TAPIModem::DisconnectDoNotDisturb` | Callee invoked Do Not Disturb feature |
| `TAPIModem::DisconnectNumberChanged` | Destination number has been changed but automatic redirection to the new number is not provided |
| `TAPIModem::DisconnectOutOfOrder` | Remote party's device is out of order (hardware failure) |
| `TAPIModem::DisconnectQOSUnavailable` | The minimum Quality of Service couldn't be obtained |
| `TAPIModem::DisconnectTemporaryFailure` | Call was disconnected because of a temporary failure in the network |
| `TAPIModem::DisconnectedByFunction` | Call was manually disconnected using `endConnection()` |
| `TAPIModem::DisconnectUnknown` | Call was disconnected but we don't know the reason |

## Methods, properties and signals
Below, you can find all the necessary methods and properties of the `TAPIModem` class. Of course, it derives from `QIODevice`, so reading, writing and other operations uses QIODevice's respective functions like `write()` or `read()`.

| Method | Description |
|--|--|
| `TAPIModem::TAPIModem(QObject *parent = 0)` | Constructs a new TAPI application object with the given *parent* |
| `TAPIModem::~TAPIModem()` | Deinitializes TAPI subsystem, if necessary, and then destroys object |
| `qint64 TAPIModem::bytesAvailable()` | Returns the number of incoming bytes that are waiting to be read |
| `CallState TAPIModem::callState()` | Returns current call state |
| `void TAPIModem::clearError()` | Clears current error |
| `void TAPIModem::connectToNumber()` | Opens the modem and invokes connection to destination number. You can set default modem id and destination number using `setDeviceId()` and `setDestinationNumber()` |
| `void TAPIModem::connectToNumber(quint32  modemId,  QString  destNumber)` | Opens specified modem and invokes connection to specifed destination number. Provided modem id and destination number is saved for future use |
| `void TAPIModem::close()` | Invokes `endConnection()` and closes underlying QIODevice |
| `DisconnectReasonTAPIModem::disconnectReason()` | Returns current disconnect reason |
| `void TAPIModem::endConnection()` | Hangs current call and closes the modem |
| `TAPIError TAPIModem::error()` | Returns current error |
| `bool TAPIModem::initializeTAPI()` | Intializes TAPI subsystem with default app name. Returns `true` if initialized successfully, otherwise `false` |
| `bool TAPIModem::initializeTAPI(QString appName)` | Intializes TAPI subsystem with provided application name. Returns `true` if initialized successfully, otherwise `false` |
| `bool TAPIModem::isSequential() const` | Always returns `true`. Modem is a sequential device |
| `LineState TAPIModem::lineState()` | Returns current line state |
| `void TAPIModem::setDestinationNumber(QString  number)` | Sets default destination number |
| `void TAPIModem::setDeviceId(quint32  deviceId)` | Sets default modem id |
| `void TAPIModem::setFriendlyName(QString  name)` | Sets default application name |
| `TAPIState TAPIModem::tapiState()` | Returns current TAPI state |
| `bool TAPIModem::waitForConnected(int msecs = 30000)` | Waits for the `connected()` signal for `msecs` miliseconds. Returns `true` when connected and `false` when timeout or got disconnected while waiting |
| `bool TAPIModem::waitForDisconnected(int msecs = 30000)` | Waits for the `disconnected()` signal for `msecs` miliseconds. Returns `true` when disconnected and `false` when timeout |
| `bool TAPIModem::waitForReadyRead(int msecs = 30000)` | Waits for the `readyRead()` signal for `msecs` miliseconds. Returns `true` when data ready to read and `false` when timeout |

---

| Signal | Description |
|--|--|
| `callStateChanged(CallState)` | The signal is emitted after the call state changes and before closing any connections (if applicable) |
| `connected()` | The signal is emitted when the modem is definitely connected to the remote party and data is flowing |
| `disconnected()` | The signal is emitted when the modem is definitely disconnected and all internal handles are closed |
| `errorOccurred(TAPIError)` | The signal is emitted after an error occured and before closing any connections |
| `lineStateChanged(LineState)` | The signal is emitted after the line state changes and before closing any connections (if applicable) |
| `tapiStateChanged(TAPIState)` | The signal is emitted after TAPI subsystem state changes |

## Helper classes
QtTAPIModem provides two helper classes - `TAPIModemInfo` and `DialableNumberBuilder`.

**TAPIModemInfo** is used for enumerating available modems. To `QList` of `TAPIModemInfo` objects use the static `TAPIModemInfo::availableModems()` method. Each object has `qint32 deviceId()` and `QString modemName()` methods providing modem ID and modem name. 

**DialableNumberBuilder** is a simple class for building dialable phone numbers. For example:
```c++
QString number = DialableNumberBuilder().AddCountryCode(12).AddAreaCode(64).AddNumber("123123123").AddPause(10).AddNumber("1024").Build();
```
Creates string `+ 12 [64] 123123123,,,,,,,,,,1024`. Periods are used for pauses between dials. It's useful when the remote party has a DISA with IVR and you need to dial some internal number to connect to the modem.

## Installing
You can install QtTAPIModem in many ways. The library is provided as a `qmake` project, so you can build it as any other Qt library and link it statically or dynamically. 

If you want to build the qmake project as a static library, remember to add `CONFIG += staticlib` to the `QtTAPIModem.pro` file. Also add `QTTAPIMODEM_STATICALLY_LINKED` to the `DEFINES` variable, otherwise all classes will be marked as exported, which will cause issues when linking.

Since the library consists mainly of 3 files, you can also insert it directly into your project and build it statically as part of your main application. Just remember to add `QTTAPIMODEM_STATICALLY_LINKED` to your `DEFINES` and `-luser32 -ltapi32` to your `LIBS`.

## License
This library is provided under the terms of the MIT License.

The `tapiterminal` example is based on Qt's `terminal` example for the QSerialPort library so, it's licensed under their respective licenses (BSD License and Qt Company Commercial License). More information is inside the source files of the `tapiterminal` project.