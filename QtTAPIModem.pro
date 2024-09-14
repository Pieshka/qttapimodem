#
# MIT License
#
# Copyright (c) 2024 Pieszka
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

TEMPLATE = lib
QT -= gui
DEFINES += QTTAPIMODEM_LIBRARY QT_DEPRECATED_WARNINGS

TARGET = QtTAPIModem

VERSION = 1.0.0

OBJECTS_DIR = out/obj
MOC_DIR = out/generated
UI_DIR = out/generated
RCC_DIR = out/generated

SOURCES += qttapimodem.cpp

HEADERS += qttapimodem.h\
        qttapimodem_global.h

LIBS += -luser32 -ltapi32

unix {
    error(Unix systems are unsupported with this library!)
}

android {
    error(Android systems are unsupported with this library!)
}

win32 {
    LIB_TARGET = $${TARGET}.dll
}

target.path = $$PREFIX/lib
headers.path = $$PREFIX/include/QtTAPIModem
headers.files = $$HEADERS

INSTALLS += target headers
