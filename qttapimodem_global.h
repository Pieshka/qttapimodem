/*
 * SPDX-FileCopyrightText: 2024 Pieszka
 * SPDX-License-Identifier: MIT
 */

#ifndef QTTAPIMODEM_GLOBAL_H
#define QTTAPIMODEM_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(QTTAPIMODEM_STATICALLY_LINKED)
#    define QTM_EXPORT
#elif defined(QTTAPIMODEM_LIBRARY)
#    define QTM_EXPORT Q_DECL_EXPORT
#else
#    define QTM_EXPORT Q_DECL_IMPORT
#endif


#endif // QTTAPIMODEM_GLOBAL_H
