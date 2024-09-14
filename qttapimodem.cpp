/*
 * SPDX-FileCopyrightText: 2024 Pieszka
 * SPDX-License-Identifier: MIT
 */

#include "qttapimodem.h"

#if defined(QT_DEBUG) && defined(QTAPI_DEBUG)
#include <QDebug>
#endif

TAPIModem::TAPIModem(QObject *parent) : QIODevice(parent)
{
}

TAPIModem::~TAPIModem()
{
    if(tapiStateFlag == Uninitialized) return;

    /* We need to deinitialize TAPI */
    deinitializeTAPI();
}

bool TAPIModem::initializeTAPI()
{
    return initializeTAPI(friendlyName);
}

bool TAPIModem::initializeTAPI(QString appName)
{
    if(tapiStateFlag == Initialized) return false;

#if defined(QT_DEBUG) && defined(QTAPI_DEBUG)
    qDebug() << "QTapiModem - initializeTAPI: Starting TAPI initialization";
#endif

    LONG ret = 0;
    LINEINITIALIZEEXPARAMS lineInitParams = {};

    /* Set lineInitializeEX params */
    lineInitParams.dwTotalSize = sizeof(lineInitParams);
    lineInitParams.dwOptions = LINEINITIALIZEEXOPTION_USEEVENT;

    /* Create TAPI supported API DWORD */
    DWORD tapiSupportedApi = TAPI_SUPPORTED_API;

    /* Try initializing TAPI */
    do
    {
        ret = lineInitializeEx(&hLineApp, NULL, NULL, appName.toStdWString().c_str(), &dwDeviceNumber, &tapiSupportedApi, &lineInitParams);

#if defined(QT_DEBUG) && defined(QTAPI_DEBUG)
        qDebug() << "QTapiModem - initializeTAPI: lineInitializeEx returned with value: " << ret;
#endif
        if(ret < 0 && ret != (LONG)LINEERR_REINIT)
        {
            errFlag = InitError;
            emit errorOccurred(errFlag);
            return false;
        }
    }
    while(ret == (LONG)LINEERR_REINIT);

    /* Prepare event notifier */
    tapiEventNotifier = new QWinEventNotifier(this);
    connect(tapiEventNotifier, &QWinEventNotifier::activated, this, &TAPIModem::on_TAPIevent);
    tapiEventNotifier->setHandle(lineInitParams.Handles.hEvent);
    tapiEventNotifier->setEnabled(true);

    /* Change state */
    tapiStateFlag = Initialized;
    emit tapiStateChanged(tapiStateFlag);

#if defined(QT_DEBUG) && defined(QTAPI_DEBUG)
    qDebug() << "QTapiModem - initializeTAPI: initialized successfully!";
#endif

    return true;
}

void TAPIModem::connectToNumber()
{
    if(tapiStateFlag == Uninitialized) return;

    LONG ret = 0;
    DWORD dwLocalAPIVersion;
    LINEEXTENSIONID lineExtensionId = {};
    LINECALLPARAMS lineCallParams = {};

    /* Try to negotiate API version we will be using */
    ret = lineNegotiateAPIVersion(hLineApp, dwDeviceId, 0x0010004, TAPI_SUPPORTED_API, &dwLocalAPIVersion, &lineExtensionId);

#if defined(QT_DEBUG) && defined(QTAPI_DEBUG)
    qDebug() << "QTapiModem - connectToNumber: lineNegotiateAPIVersion returned with value:" << ret;
#endif
    if(ret < 0)
    {
        errFlag = ret == (LONG)LINEERR_NODEVICE || ret == (LONG)LINEERR_BADDEVICEID ? NoDeviceFoundError : NegotiationError;
        emit errorOccurred(errFlag);
        return;
    }

    /* Try to open TAPI line device if not opened */
    lineMutex.lock();
    if(hlDevice == 0)
    {
        ret = lineOpen(hLineApp, dwDeviceId, &hlDevice, dwLocalAPIVersion, 0, 0, LINECALLPRIVILEGE_OWNER, LINEMEDIAMODE_DATAMODEM, 0);

#if defined(QT_DEBUG) && defined(QTAPI_DEBUG)
        qDebug() << "QTapiModem - connectToNumber: lineOpen returned with value:" << ret;
#endif
        if(ret < 0)
        {
            lineMutex.unlock();
            errFlag = ret == (LONG)LINEERR_NODEVICE || ret == (LONG)LINEERR_BADDEVICEID ? NoDeviceFoundError : LineOpenError;
            emit errorOccurred(errFlag);
            return;
        }
        /* We got the line opened, so let's inform about it */
        lineStateFlag = LineOpened;
        emit lineStateChanged(lineStateFlag);

        ret = lineSetStatusMessages(hlDevice, LINEDEVSTATE_CONNECTED | LINEDEVSTATE_DISCONNECTED | LINEDEVSTATE_OUTOFSERVICE | LINEDEVSTATE_MAINTENANCE | LINEDEVSTATE_CLOSE | LINEDEVSTATE_REINIT | LINEDEVSTATE_REMOVED, 0);

#if defined(QT_DEBUG) && defined(QTAPI_DEBUG)
        qDebug() << "QTapiModem - connectToNumber: lineSetStatusMessages returned with value:" << ret;
#endif
        if(ret < 0)
        {
            lineMutex.unlock();
            errFlag = ret == (LONG)LINEERR_NODEVICE || ret == (LONG)LINEERR_BADDEVICEID ? NoDeviceFoundError : OperationError;
            emit errorOccurred(errFlag);
            return;
        }
        lineMutex.unlock();

        lineCallParams.dwTotalSize = sizeof(LINECALLPARAMS);
        lineCallParams.dwBearerMode = LINEBEARERMODE_VOICE;
        lineCallParams.dwMediaMode = LINEMEDIAMODE_DATAMODEM;
        lineCallParams.dwCallParamFlags = LINECALLPARAMFLAGS_IDLE;
        lineCallParams.dwAddressMode = LINEADDRESSMODE_ADDRESSID;
        lineCallParams.dwAddressID = 0;
    }

    /* Try to make a call, if one is not existent */
    callMutex.lock();
    if(hcCurrentCall == 0)
    {
        ret = lineMakeCall(hlDevice, &hcCurrentCall, destinationNumber.toStdWString().c_str(),0,&lineCallParams);

#if defined(QT_DEBUG) && defined(QTAPI_DEBUG)
        qDebug() << "QTapiModem - connectToNumber: lineMakeCall returned with value:" << ret << ", called number:" << destinationNumber.toStdString().c_str();
#endif
        if(ret < 0)
        {
            callMutex.unlock();
            errFlag = CallMakeError;
            emit errorOccurred(errFlag);
            return;
        }
    }
    callMutex.unlock();
}

void TAPIModem::connectToNumber(quint32 modemId, QString destNumber)
{
    setDeviceId(modemId);
    setDestinationNumber(destNumber);
    connectToNumber();
}

void TAPIModem::endConnection()
{
    /* Just hangup the call and reset states */
    hangupCall();

    callStateFlag = CallDefaultState;
    lineStateFlag = LineClosed;
    disconnectFlag = DisconnectedByFunction;
    emit callStateChanged(callStateFlag);
    emit lineStateChanged(lineStateFlag);
}

qint64 TAPIModem::bytesAvailable() const
{
    return modemReadBuffer.size() + QIODevice::bytesAvailable();
}

bool TAPIModem::waitForReadyRead(int msecs)
{
    /* Return false if all signs shows that the call is disconnected */
    if(tapiStateFlag == Uninitialized && callStateFlag != CallConnected && lineStateFlag != LineOpened && hCommFile == INVALID_HANDLE_VALUE) return false;

    /* Create timer and an Event Loop */
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(this, &QIODevice::readyRead, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    /* And now trigger! */
    timer.start(msecs);
    loop.exec();

    /* We got a timeout */
    if(!timer.isActive())
        return false;

    /* We didn't get a timeout */
    return true;
}

bool TAPIModem::waitForConnected(int msecs)
{
    /* Return false if TAPI is uninitialized */
    if(tapiStateFlag == Uninitialized) return false;


    /* Create timer and an Event Loop */
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(this, &TAPIModem::connected, &loop, &QEventLoop::quit);
    connect(this, &TAPIModem::disconnected, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    /* And now trigger! */
    timer.start(msecs);
    loop.exec();

    /* We got a timeout */
    if(!timer.isActive() || callStateFlag != CallConnected)
        return false;

    /* We didn't get a timeout */
    return true;
}

bool TAPIModem::waitForDisconnected(int msecs)
{
    /* Return false if all signs shows that the call is disconnected */
    if(tapiStateFlag == Uninitialized || callStateFlag != CallConnected || lineStateFlag != LineOpened || hCommFile == INVALID_HANDLE_VALUE) return true;


    /* Create timer and an Event Loop */
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(this, &TAPIModem::disconnected, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    /* And now trigger! */
    timer.start(msecs);
    loop.exec();

    /* We got a timeout */
    if(!timer.isActive())
        return false;

    /* We didn't get a timeout */
    return true;
}

void TAPIModem::close()
{
    endConnection();
    QIODevice::close();
}

void TAPIModem::shutdownTAPI()
{
    if(tapiStateFlag == Uninitialized) return;

    hangupCall();
    deinitializeTAPI();
}

void TAPIModem::deinitializeTAPI()
{
    lineAppMutex.lock();
    lineShutdown(hLineApp);
    hLineApp = 0;
    lineAppMutex.unlock();
    delete tapiEventNotifier;
    tapiEventNotifier = nullptr;
    tapiStateFlag = Uninitialized;
    emit tapiStateChanged(tapiStateFlag);

#if defined(QT_DEBUG) && defined(QTAPI_DEBUG)
    qDebug() << "QTapiModem - deinitializeTAPI: TAPI is shut down";
#endif
}

void TAPIModem::hangupCall()
{
    if(tapiStateFlag == Uninitialized) return;

#if defined(QT_DEBUG) && defined(QTAPI_DEBUG)
    qDebug() << "QTapiModem - hangupCall: Starting call hangup";
#endif

    LONG ret = 0;
    LPLINECALLSTATUS lpLineCallStatus = NULL;
    DWORD dwSizeOfLineCallStatus = sizeof(LINECALLSTATUS) + 1024;

    deinitializeCommPort();

#if defined(QT_DEBUG) && defined(QTAPI_DEBUG)
    qDebug() << "QTapiModem - hangupCall: COM port deinitialized";
#endif
    /* If call is in progress, deallocate it */
    callMutex.lock();
    if(hcCurrentCall)
    {
        /* Let's get call status */
#if defined(QT_DEBUG) && defined(QTAPI_DEBUG)
        qDebug() << "QTapiModem - hangupCall: Aquiring call state";
#endif
        /* Beware, here comes some C/Microsoft sorcery using memory allocation */
        lpLineCallStatus = (LPLINECALLSTATUS)LocalAlloc(LPTR, dwSizeOfLineCallStatus);
        lpLineCallStatus->dwTotalSize = dwSizeOfLineCallStatus;
        do
        {
            ret = lineGetCallStatus(hcCurrentCall, lpLineCallStatus);
            if(ret < 0)
            {
                /* We got an error, now let's handle this */
                LocalFree(lpLineCallStatus);
                errFlag = CallStatusAquireError;
                emit errorOccurred(errFlag);

                deinitializeTAPI();
                break;
            }
            if (lpLineCallStatus->dwTotalSize < lpLineCallStatus->dwNeededSize)
            {
                dwSizeOfLineCallStatus = lpLineCallStatus->dwNeededSize;
                lpLineCallStatus = (LPLINECALLSTATUS)LocalReAlloc((LPLINECALLSTATUS)lpLineCallStatus,dwSizeOfLineCallStatus,LMEM_MOVEABLE);
            }
        }
        while(lpLineCallStatus->dwTotalSize < lpLineCallStatus->dwNeededSize);
#if defined(QT_DEBUG) && defined(QTAPI_DEBUG)
        qDebug() << "QTapiModem - hangupCall: Call state aquired: " << lpLineCallStatus->dwCallState;
#endif
        if(!(lpLineCallStatus->dwCallState & LINECALLSTATE_IDLE))
        {
            /* If call is not in Idle state, just drop it.
             *
             * To be honest we should wait for LINEREPLY,
             * if the call was successfully dropped, but
             * that would mean we will be blocking the thread.
             * Although if we are here that means the call is
             * in progress (so it will successfully drop) or
             * the call is already dropped. Also we will try
             * deallocating the call until success or error.
             *
             * If you want to be 100% safe you should modify it.
             * See Microsoft's example "TAPICOMM".
             */
            lineDrop(hcCurrentCall, NULL, 0);
#if defined(QT_DEBUG) && defined(QTAPI_DEBUG)
            qDebug() << "QTapiModem - hangupCall: line dropped if not already";
#endif
        }
        /* Try to deallocate the call */
        do
        {
            ret = lineDeallocateCall(hcCurrentCall);
            if(ret < 0 && ret != (LONG)LINEERR_INVALCALLSTATE)
            {
                /* We got an error, now let's handle this */
                errFlag = CallDeallocationError;
                emit errorOccurred(errFlag);

                deinitializeTAPI();
                return;
            }
        }
        while(ret < 0);
#if defined(QT_DEBUG) && defined(QTAPI_DEBUG)
        qDebug() << "QTapiModem - hangupCall: call deallocated";
#endif
    }
    callMutex.unlock();

    lineMutex.lock();
    /* Now let's try to close the line */
    if(hlDevice)
    {
        /* Try to close the line */
        ret = lineClose(hlDevice);
        if(ret < 0)
        {
            /* We got an error, now let's handle this */
            errFlag = LineDeallocationError;
            emit errorOccurred(errFlag);

            deinitializeTAPI();
            return;
        }
    }
#if defined(QT_DEBUG) && defined(QTAPI_DEBUG)
    qDebug() << "QTapiModem - hangupCall: line closed";
#endif
    lineMutex.unlock();

    hcCurrentCall = 0;
    hlDevice = 0;

#if defined(QT_DEBUG) && defined(QTAPI_DEBUG)
    qDebug() << "QTapiModem - hangupCall: hangup completed!";
#endif
    /* We are disconnected by now */
    emit disconnected();
}

void TAPIModem::deinitializeCommPort()
{
    /* First we need to flush file buffers */
    FlushFileBuffers(hCommFile);

    /* Now we need to cancel any pending IO operations */
    CancelIo(hCommFile);

    /* Now we need to close the handle. It will prevent any futher IO operations */
    CloseHandle(hCommFile);
    hCommFile = INVALID_HANDLE_VALUE;

    /* Delete our notifier */
    if(commIOEventNotifier)
    {
        commIOEventNotifier->setEnabled(false);
        commIOEventNotifier->deleteLater();
        commIOEventNotifier = 0;
    }

    /* Now close any pending event handlers on our pending writes list */
    foreach(OVERLAPPED *o, pendingOverlappedWrites)
    {
        CloseHandle(o->hEvent);
        delete o;
    }
    pendingOverlappedWrites.clear();

#if defined(QT_DEBUG) && defined(QTAPI_DEBUG)
    qDebug() << "QTapiModem - deinitializeCommPort: COM port deinitialized!";
#endif
}

qint64 TAPIModem::comBytesAvailable()
{
    DWORD errors;
    COMSTAT status;

    commMutex.lock();
    if(ClearCommError(hCommFile, &errors, &status))
    {
        commMutex.unlock();
        return status.cbInQue;
    }

    commMutex.unlock();
    return -1;
}

qint64 TAPIModem::readData(char *data, qint64 maxlen)
{
    /* Check if maxlen is not bigger than available buffer size */
    qint64 n = qMin(maxlen, (qint64)modemReadBuffer.size());

    /* No data, so return */
    if(!n)
        return 0;

    /* Try to aquire n bytes */
    if (!bufferSem.tryAcquire(n))
        return 0;

    /* Copy data to QIODevice buffer that was provided */
    for(qint64 i = 0; i < n; i++)
        data[i] = modemReadBuffer.at(i);

    /* Erase data from our internal buffer */
    modemReadBuffer.remove(0, n);

    return n;
}

qint64 TAPIModem::writeData(const char *data, qint64 len)
{
    DWORD lastError = 0;
    OVERLAPPED *newOverlappedWrite = new OVERLAPPED;
    ZeroMemory(newOverlappedWrite, sizeof(OVERLAPPED));
    newOverlappedWrite->hEvent = CreateEvent(NULL, true, false, NULL);
    commMutex.lock();
    if(WriteFile(hCommFile, (void *)data, (DWORD)len, NULL, newOverlappedWrite))
    {
        /* Write finished now, we can delete our OVERLAPPED */
        CloseHandle(newOverlappedWrite->hEvent);
        delete newOverlappedWrite;
        commMutex.unlock();
    }
    else if((lastError = GetLastError()) == ERROR_IO_PENDING)
    {
        /* Writing started asynchronously */
        pendingOverlappedWrites.append(newOverlappedWrite);
        commMutex.unlock();
    }
    else
    {
#if defined(QT_DEBUG) && defined(QTAPI_DEBUG)
        qDebug() << "QTapiModem - writeData: we got an error when writing data: " << lastError;
#endif
        /* We got an error */
        CancelIo(newOverlappedWrite->hEvent);
        CloseHandle(newOverlappedWrite->hEvent);
        delete newOverlappedWrite;
        commMutex.unlock();

        errFlag = CommWriteError;
        emit errorOccurred(errFlag);

        hangupCall();
        return -1;
    }

    return len;
}

void TAPIModem::initializeCommPort()
{
    /* If there is no handle there is no need to initialize the port */
    if(hCommFile == NULL) return;
    /* Return if the handle is invalid */
    if(GetFileType(hCommFile) != FILE_TYPE_CHAR) return;

#if defined(QT_DEBUG) && defined(QTAPI_DEBUG)
    qDebug() << "QTapiModem - initializeCommPort: started comm port initialization";
#endif

    COMMTIMEOUTS commTimeouts;
    DCB dcb;
    COMMPROP commProp;
    DWORD dwEvtMask;

    /* Get configuration of the port - although most of it is already set through TAPI */
    commMutex.lock();
    GetCommState(hCommFile, &dcb);
    GetCommProperties(hCommFile, &commProp);
    GetCommMask(hCommFile, &dwEvtMask);
    GetCommTimeouts(hCommFile, &commTimeouts);
    commMutex.unlock();

    /* Next we need to set a few options related to timeouts
     * it's safe to assume that after 250ms if we don't get
     * any more characters the remote end finished transmitting
     */
    commTimeouts.ReadIntervalTimeout = 250;
    commTimeouts.ReadTotalTimeoutMultiplier = 0;
    commTimeouts.ReadTotalTimeoutConstant = 0;
    commTimeouts.WriteTotalTimeoutMultiplier = 0;
    commTimeouts.WriteTotalTimeoutConstant = 0;

    commMutex.lock();
    SetCommTimeouts(hCommFile, &commTimeouts);
    commMutex.unlock();

    /* Errors on modem line are common */
    dcb.fAbortOnError = false;
    commMutex.lock();
    SetCommState(hCommFile, &dcb);
    commMutex.unlock();

    /* Setup necessary structure for overlapped IO */
    ZeroMemory(&overlap, sizeof(OVERLAPPED));
    overlap.hEvent = CreateEvent(NULL, true, false, NULL);

    /* Set CommMask for the port */
    commMutex.lock();
    SetCommMask(hCommFile, EV_TXEMPTY | EV_RXCHAR);
    commMutex.unlock();

    /* Set initial event notification */
    commIOEventNotifier = new QWinEventNotifier(overlap.hEvent, this);
    connect(commIOEventNotifier, &QWinEventNotifier::activated, this, &TAPIModem::on_COMevent);
    commIOEventNotifier->setEnabled(true);

    WaitCommEvent(hCommFile,&receivedEventMask,&overlap);

#if defined(QT_DEBUG) && defined(QTAPI_DEBUG)
    qDebug() << "QTapiModem - initializeCommPort: comm port initialized";
#endif

    /* Now we are connected */
    QIODevice::open(QIODevice::ReadWrite);
    emit connected();
}

/* Now hold on, because here comes a spaghetti of a function */
void TAPIModem::on_TAPIevent()
{
    LONG ret = 0;
    DWORD dwTimeout = 10000;
    LINEMESSAGE lmTapiMessage = {};

    lineAppMutex.lock();
    ret = lineGetMessage(hLineApp, &lmTapiMessage, dwTimeout);
    lineAppMutex.unlock();

    switch(ret)
    {
    case LINEERR_INVALPOINTER:
    case LINEERR_NOMEM:
        /* Very serious errors, better shutdown TAPI */
        shutdownTAPI();
        errFlag = OperationError;
        emit errorOccurred(errFlag);
        return;
    case LINEERR_INVALAPPHANDLE:
    case LINEERR_OPERATIONFAILED:
        /* TAPI has just shutdown or there aren't queued messages (which is weird) */
        return;
    default:
        break;
    }

    /* Check if LINE_CALLSTATE does apply to our call */
    if((HCALL)lmTapiMessage.hDevice != hcCurrentCall)
        return;

#if defined(QT_DEBUG) && defined(QTAPI_DEBUG)
    qDebug() << "QTapiModem - on_TAPIevent: TAPI event received:" << lmTapiMessage.dwMessageID << " Param1:" << lmTapiMessage.dwParam1 << " Param2:" << lmTapiMessage.dwParam2 << " Param3:" << lmTapiMessage.dwParam3;
#endif

    /* Now let's switch over the message */
    switch (lmTapiMessage.dwMessageID)
    {
    /* First we have callstate changes */
    case LINE_CALLSTATE:
    {
        switch(lmTapiMessage.dwParam1)
        {
        /* For Dialing we will only inform. No need to take actions. */
        case LINECALLSTATE_DIALING:
            callStateFlag = CallDialing;
            emit callStateChanged(callStateFlag);

            break;
        case LINECALLSTATE_BUSY:
            callStateFlag = CallBusy;
            emit callStateChanged(callStateFlag);

            hangupCall();
            break;
        case LINECALLSTATE_IDLE:
            callStateFlag = CallIdle;
            emit callStateChanged(callStateFlag);

            hangupCall();
            break;
        case LINECALLSTATE_SPECIALINFO:
            callStateFlag = CallCannotDial;
            emit callStateChanged(callStateFlag);

            hangupCall();
            break;
        /* Let the nesting begin! */
        case LINECALLSTATE_DISCONNECTED:
            switch(lmTapiMessage.dwParam2)
            {
            case LINEDISCONNECTMODE_NORMAL: disconnectFlag = DisconnectByRemote; break;
            case LINEDISCONNECTMODE_BUSY: disconnectFlag = DisconnectBusy; break;
            case LINEDISCONNECTMODE_NOANSWER: disconnectFlag = DisconnectNoAnswer; break;
            case LINEDISCONNECTMODE_REJECT: disconnectFlag = DisconnectReject; break;
            case LINEDISCONNECTMODE_PICKUP: disconnectFlag = DisconnectPickup; break;
            case LINEDISCONNECTMODE_FORWARDED: disconnectFlag = DisconnectForwarded; break;
            case LINEDISCONNECTMODE_BADADDRESS: disconnectFlag = DisconnectBadAddress; break;
            case LINEDISCONNECTMODE_UNREACHABLE: disconnectFlag = DisconnectUnreachable; break;
            case LINEDISCONNECTMODE_CONGESTION: disconnectFlag = DisconnectCongestion; break;
            case LINEDISCONNECTMODE_INCOMPATIBLE: disconnectFlag = DisconnectIncompatible; break;
            case LINEDISCONNECTMODE_UNAVAIL: disconnectFlag = DisconnectUnavailable; break;
            case LINEDISCONNECTMODE_NODIALTONE: disconnectFlag = DisconnectUnavailable; break;
            case LINEDISCONNECTMODE_BLOCKED: disconnectFlag = DisconnectBlocked; break;
            case LINEDISCONNECTMODE_CANCELLED: disconnectFlag = DisconnectCancelled; break;
            case LINEDISCONNECTMODE_DONOTDISTURB: disconnectFlag = DisconnectDoNotDisturb; break;
            case LINEDISCONNECTMODE_NUMBERCHANGED: disconnectFlag = DisconnectNumberChanged; break;
            case LINEDISCONNECTMODE_OUTOFORDER: disconnectFlag = DisconnectOutOfOrder; break;
            case LINEDISCONNECTMODE_QOSUNAVAIL: disconnectFlag = DisconnectQOSUnavailable; break;
            case LINEDISCONNECTMODE_TEMPFAILURE: disconnectFlag = DisconnectTemporaryFailure; break;
            default: disconnectFlag = DisconnectUnknown; break;
            }
            callStateFlag = CallDisconnected;
            emit callStateChanged(callStateFlag);

            hangupCall();
            break;
        /* Here comes the most important part */
        case LINECALLSTATE_CONNECTED:
        {
            /* Sometimes is possible for receiving multiple CONNECTED messages.
             * If we are connected it's better just to skip all of this.
             */
            if(callStateFlag == CallConnected) break;

#if defined(QT_DEBUG) && defined(QTAPI_DEBUG)
            qDebug() << "QTapiModem - on_TAPIevent: Starting connection procedure";
#endif

            LONG ret = 0;
            LPVARSTRING lpVarString = NULL;
            DWORD dwSizeOfVarStr = sizeof(VARSTRING) + 1024;

            /* Beware, here comes some C/Microsoft sorcery using memory allocation */
            lpVarString = (LPVARSTRING)LocalAlloc(LPTR, dwSizeOfVarStr);
            lpVarString->dwTotalSize = dwSizeOfVarStr;

            bool breakSwitch = false;
            do
            {
                callMutex.lock();
                ret = lineGetID((HLINE)NULL, 0, hcCurrentCall, LINECALLSELECT_CALL, lpVarString, L"comm/datamodem");
                callMutex.unlock();
                if(ret != 0)
                {
                    /* We got an error, now let's handle this */
                    commMutex.lock();
                    if(hCommFile)
                        CloseHandle(hCommFile);
                    commMutex.unlock();
                    LocalFree(lpVarString);

                    errFlag = CommAquireError;
                    emit errorOccurred(errFlag);

                    hangupCall();
                    breakSwitch = true;
                    break;
                }
                if (lpVarString->dwTotalSize < lpVarString->dwNeededSize)
                {
                    dwSizeOfVarStr = lpVarString->dwNeededSize;
                    lpVarString = (LPVARSTRING)LocalReAlloc((LPVARSTRING)lpVarString,dwSizeOfVarStr,LMEM_MOVEABLE);
                }
            }
            while(lpVarString->dwTotalSize < lpVarString->dwNeededSize);
            if(breakSwitch) break;

            /* Now we got the necessary handle! */
            commMutex.lock();
            hCommFile = *(HANDLE *)((char *)lpVarString + lpVarString->dwStringOffset);
            commMutex.unlock();
            initializeCommPort();

#if defined(QT_DEBUG) && defined(QTAPI_DEBUG)
            qDebug() << "QTapiModem - on_TAPIevent: call start procedure finished successfully";
#endif

            callStateFlag = CallConnected;
            emit callStateChanged(callStateFlag);
            break;
        }
        default:
            /* We do nothing */
            break;
        }
        break;
    }
    case LINE_CLOSE:
        /* Line just got shutdown. Let's clean this and that */
        lineStateFlag = LineClosed;
        emit lineStateChanged(lineStateFlag);

        hangupCall();
        break;
    case LINE_LINEDEVSTATE:
        /* Some change on the line occured. Better check */
        switch (lmTapiMessage.dwParam1)
        {
        /* Oh no. TAPI wants to reload. We must obey. */
        case LINEDEVSTATE_REINIT:
            switch(lmTapiMessage.dwParam2)
            {
            /* TAPI doesn't provide us any reason for why. They want us to shutdown. */
            case 0:
                lineStateFlag = LineReinitialization;
                emit lineStateChanged(lineStateFlag);

                shutdownTAPI();
            /* We don't care for other reasons */
            default:
                break;
            }
            break;
        case LINEDEVSTATE_OUTOFSERVICE:
            lineStateFlag = LineOutOfService;
            emit lineStateChanged(lineStateFlag);

            hangupCall();
            break;
        case LINEDEVSTATE_DISCONNECTED:
            lineStateFlag = LineDisconnected;
            emit lineStateChanged(lineStateFlag);

            hangupCall();
            break;
        case LINEDEVSTATE_MAINTENANCE:
            lineStateFlag = LineMaintenance;
            emit lineStateChanged(lineStateFlag);

            hangupCall();
            break;
        case LINEDEVSTATE_REMOVED:
            lineStateFlag = LineDeviceRemoved;
            emit lineStateChanged(lineStateFlag);

            hangupCall();
            break;
        default:
            break;
        }
        break;
    case LINE_REPLY:
        if(lmTapiMessage.dwParam2 != 0)
        {
            /* Whatever we were trying to do. Failed. Just close the line and forget about everything. */
#if defined(QT_DEBUG) && defined(QTAPI_DEBUG)
            qDebug() << "QTapiModem - on_TAPIevent: One of the asynch invoked TAPI functions failed! RequestId: " << lmTapiMessage.dwParam1 << " returned code: " << lmTapiMessage.dwParam2;
#endif
            errFlag = LineReplyError;
            emit errorOccurred(errFlag);

            hangupCall();
            break;
        }
        emit lineReplyOccured(QPrivateSignal(), lmTapiMessage.dwParam1, lmTapiMessage.dwParam2);
        break;
    case LINE_CREATE:
        /* It informs us that new devices appeared. Not that we are instrested in. */
        break;
    default:
        /* Ignore rest */
        break;
    }

}

void TAPIModem::on_COMevent()
{
#if defined(QT_DEBUG) && defined(QTAPI_DEBUG)
    qDebug() << "QTapiModem - on_COMevent: COM port event received!";
#endif

    if (receivedEventMask & EV_RXCHAR)
        com_readReady();
    if (receivedEventMask & EV_TXEMPTY)
    {
#if defined(QT_DEBUG) && defined(QTAPI_DEBUG)
        qDebug() << "QTapiModem - on_COMevent: Some sending has been completed";
#endif

        /* Write completed. Now we need to clear the OVERLAPPED write list. */
        qint64 totalBytesWritten = 0;
        QList<OVERLAPPED *> overlappedWritesToDelete;
        commMutex.lock();
        foreach(OVERLAPPED *o, pendingOverlappedWrites)
        {
            DWORD bytes = 0;
            bool getOverlappedResult = GetOverlappedResult(hCommFile, o, &bytes, false);
            DWORD getLastError = GetLastError();
            if(getOverlappedResult)
            {
                totalBytesWritten += bytes;
                overlappedWritesToDelete.append(o);
            }
            else if (getLastError != ERROR_IO_INCOMPLETE)
            {
#if defined(QT_DEBUG) && defined(QTAPI_DEBUG)
                qDebug() << "QTapiModem - on_COMevent: An error occured when writing to device. Error code: " << getLastError;
#endif
                commMutex.unlock();
                /* We got an error when writing. Better close connection */
                errFlag = CommWriteError;

                emit errorOccurred(errFlag);
                hangupCall();
                return;
            }
        }
        foreach(OVERLAPPED *o, overlappedWritesToDelete)
        {
            OVERLAPPED *v = pendingOverlappedWrites.takeAt(pendingOverlappedWrites.indexOf(o));
            pendingOverlappedWrites.removeOne(v);
            CloseHandle(v->hEvent);
            delete v;
        }

        commMutex.unlock();
        emit bytesWritten(totalBytesWritten);
    }

    WaitCommEvent(hCommFile, &receivedEventMask, &overlap);
}

void TAPIModem::com_readReady()
{
    DWORD bytesReturned = 0;
    qint64 bytesAvailable = 0;
    char *buff;

    if((bytesAvailable = comBytesAvailable()) > 0)
    {
#if defined(QT_DEBUG) && defined(QTAPI_DEBUG)
        qDebug() << "QTapiModem - on_COMevent: Data is ready to be read: " << bytesAvailable << " bytes.";
#endif
        /* Create temporary buffer for our data and read it */
        buff = new char [bytesAvailable];
        OVERLAPPED overlappedRead;
        ZeroMemory(&overlappedRead, sizeof(OVERLAPPED));
        if(!ReadFile(hCommFile, (void *)buff, (DWORD)bytesAvailable, &bytesReturned, &overlappedRead))
        {
            if(GetLastError() == ERROR_IO_PENDING)
                GetOverlappedResult(hCommFile, &overlappedRead, &bytesReturned, true);
            else
            {
                /* We got an error when reading. Better close connection */
                hangupCall();
                errFlag = CommReadError;
                emit errorOccurred(errFlag);
            }
        }

        /* Now let's insert the data inside our main buffer */
        modemReadBuffer.append(buff, bytesReturned);

        /* Release bytesReturned bytes */
        bufferSem.release(bytesReturned);

        /* Delete the temporary buffer */
        delete buff;

        /* Emit readyRead signal */
        emit readyRead();
    }
}

QList<TAPIModemInfo> TAPIModemInfo::availableModems()
{
    QList<TAPIModemInfo> list;
    LONG ret;
    HLINEAPP hLineApp;
    DWORD tapiVersion = TAPI_SUPPORTED_API;
    LPCWSTR appName = (const wchar_t *)TAPI_FRIENDLYNAME;
    DWORD deviceCount = 0;

    LINEINITIALIZEEXPARAMS lineInit = {};
    lineInit.dwTotalSize = sizeof(LINEINITIALIZEEXPARAMS);
    lineInit.dwOptions = LINEINITIALIZEEXOPTION_USEEVENT;

    do
    {
        ret = lineInitializeEx(&hLineApp, NULL, NULL, appName, &deviceCount, &tapiVersion, &lineInit);
        if(ret != 0 && ret != (LONG)LINEERR_REINIT)
            return list;
    }
    while(ret == (LONG)LINEERR_REINIT);

    LPLINEDEVCAPS lpLineDevCaps = (LPLINEDEVCAPS)LocalAlloc(LPTR, 4096);
    lpLineDevCaps->dwTotalSize = 4096;
    LINEEXTENSIONID lineExtensionId = {};

    for(DWORD devId = 0; devId < deviceCount; devId++ )
    {
        bool devCapsError = false;

        /* Try to negotiate API version */
        DWORD dwLocalAPIVersion;
        if(lineNegotiateAPIVersion(hLineApp, devId, 0x0010004, tapiVersion, &dwLocalAPIVersion, &lineExtensionId)) continue;
        do
        {
            /* Get device capabilities */
            ret = lineGetDevCaps(hLineApp, devId, dwLocalAPIVersion, 0, lpLineDevCaps);
            if(ret)
            {
                /* We got an error. Skip deviceId */
                devCapsError = true;
                break;
            }
            if (lpLineDevCaps->dwTotalSize < lpLineDevCaps->dwNeededSize)
            {
                lpLineDevCaps = (LPLINEDEVCAPS)LocalReAlloc((LPLINEDEVCAPS)lpLineDevCaps,lpLineDevCaps->dwNeededSize,LMEM_MOVEABLE);
                lpLineDevCaps->dwTotalSize = lpLineDevCaps->dwNeededSize;
                continue;
            }
            break;
        }
        while(lpLineDevCaps->dwTotalSize < lpLineDevCaps->dwNeededSize);

        /* Continue if cannot get device capatibilities */
        if(devCapsError) continue;

        /* Continue if select device is not a data modem */
        if(!(lpLineDevCaps->dwMediaModes & LINEMEDIAMODE_DATAMODEM)) continue;

        /* Sometimes a device can present itself as data modem while it is not one. lineOpen tells us if a device is really a modem */
        HLINE hLine;
        if(lineOpen(hLineApp, devId, &hLine, dwLocalAPIVersion, 0, 0, LINECALLPRIVILEGE_OWNER, LINEMEDIAMODE_DATAMODEM, 0)) { lineClose(hLine); continue; }
        lineClose(hLine);

        /* Get modem name if it exists */
        QString modemName = QString("NONAME MODEM " + QString::number(devId));
        if(lpLineDevCaps->dwLineNameSize && lpLineDevCaps->dwLineNameOffset)
        {
            /* Ugly but necessary */
            TCHAR name[512];
            CopyMemory((PVOID)name, (PVOID) ((BYTE *)lpLineDevCaps + lpLineDevCaps -> dwLineNameOffset),lpLineDevCaps->dwLineNameSize);
            modemName = QString::fromWCharArray((const wchar_t *)name, (lpLineDevCaps->dwLineNameSize-1) / 2);
        }

        TAPIModemInfo modem;
        modem.name = modemName;
        modem.devId = (qint32)devId;

        list.append(modem);
    }

    lineShutdown(hLineApp);

    return list;
}

DialableNumberBuilder &DialableNumberBuilder::AddCountryCode(int countryCode)
{
    _dialableNumber.append(QString("+ %1 ").arg(countryCode));
    return *this;
}

DialableNumberBuilder &DialableNumberBuilder::AddAreaCode(int areaCode)
{
    _dialableNumber.append(QString("[%1] ").arg(areaCode));
    return *this;
}

DialableNumberBuilder &DialableNumberBuilder::AddNumber(QString number)
{
    _dialableNumber.append(number);
    return *this;
}

DialableNumberBuilder &DialableNumberBuilder::AddPause(int durationSec)
{
    if(durationSec > 0)
        for(int i = 0; i < durationSec; i++)
            _dialableNumber.append(QString(","));
    return *this;
}

QString DialableNumberBuilder::Build()
{
    return _dialableNumber;
}

