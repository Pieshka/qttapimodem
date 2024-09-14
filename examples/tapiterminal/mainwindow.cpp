/****************************************************************************
**
** Copyright (C) 2012 Denis Shienkov <denis.shienkov@gmail.com>
** Copyright (C) 2012 Laszlo Papp <lpapp@kde.org>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSerialPort module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "console.h"
#include "settingsdialog.h"

#include <QMessageBox>
#include <QLabel>
#include "qttapimodem.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    console = new Console;
    console->setEnabled(false);
    setCentralWidget(console);

    modem = new TAPIModem(this);
    modem->initializeTAPI(QString("TAPI_Terminal_Example"));

    settings = new SettingsDialog;

    ui->actionConnect->setEnabled(true);
    ui->actionDisconnect->setEnabled(false);
    ui->actionQuit->setEnabled(true);
    ui->actionConfigure->setEnabled(true);

    status = new QLabel;
    ui->statusBar->addWidget(status);

    initActionsConnections();

    connect(modem, &TAPIModem::errorOccurred, this, &MainWindow::handleError);
    connect(modem, &TAPIModem::callStateChanged, this, &MainWindow::handleCallState);
    connect(modem, &TAPIModem::connected, this, &MainWindow::handleConnected);
    connect(modem, &TAPIModem::disconnected, this, &MainWindow::handleDisconnected);
    connect(modem, &TAPIModem::readyRead, this, &MainWindow::readData);

    connect(console, &Console::getData, this, &MainWindow::writeData);

}

MainWindow::~MainWindow()
{
    delete settings;
    delete ui;
}

void MainWindow::dialModem()
{
    SettingsDialog::Settings p = settings->settings();
    modem->setDeviceId(p.modemId);
    modem->setDestinationNumber(p.phoneNumber);

    ui->actionConnect->setEnabled(false);
    ui->actionDisconnect->setEnabled(true);
    ui->actionConfigure->setEnabled(false);

    modem->connectToNumber();
}

void MainWindow::hangModem()
{
    modem->endConnection();
}

void MainWindow::about()
{
    QMessageBox::about(this, tr("About Simple TAPI Terminal"),
                       tr("The <b>Simple TAPI Terminal</b> example demonstrates how to "
                          "use the QTAPIModem module in modern GUI applications "
                          "using Qt, with a menu bar, toolbars, and a status bar."));
}

void MainWindow::writeData(const QByteArray &data)
{
    modem->write(data);
}

void MainWindow::readData()
{
    QByteArray data = modem->readAll();
    console->putData(data);
}

void MainWindow::handleError(TAPIModem::TAPIError error)
{
    QMessageBox::critical(this, tr("Critical Error"), QString("Error code: %1").arg((qint32)error));
    hangModem();
}

void MainWindow::handleCallState(TAPIModem::CallState callState)
{
    if(callState == TAPIModem::CallDialing)
        showStatusMessage(tr("Dialing number..."));
}

void MainWindow::handleConnected()
{
    SettingsDialog::Settings p = settings->settings();
    console->setEnabled(true);
    console->setLocalEchoEnabled(p.localEchoEnabled);
    showStatusMessage(tr("Connected to %1").arg(p.phoneNumber));
}

void MainWindow::handleDisconnected()
{
    console->setEnabled(false);
    ui->actionConnect->setEnabled(true);
    ui->actionDisconnect->setEnabled(false);
    ui->actionConfigure->setEnabled(true);
    showStatusMessage(tr("Disconnected, with reason: %1").arg(modem->disconnectReason()));
}

void MainWindow::initActionsConnections()
{
    connect(ui->actionConnect, &QAction::triggered, this, &MainWindow::dialModem);
    connect(ui->actionDisconnect, &QAction::triggered, this, &MainWindow::hangModem);
    connect(ui->actionQuit, &QAction::triggered, this, &MainWindow::close);
    connect(ui->actionConfigure, &QAction::triggered, settings, &MainWindow::show);
    connect(ui->actionClear, &QAction::triggered, console, &Console::clear);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::about);
    connect(ui->actionAboutQt, &QAction::triggered, qApp, &QApplication::aboutQt);
}

void MainWindow::showStatusMessage(const QString &message)
{
    status->setText(message);
}
