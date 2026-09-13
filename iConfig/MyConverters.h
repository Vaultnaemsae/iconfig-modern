/*
;iConfig source code and documentation is released under a GPLv3 license. 
;
; A copy is available from the Open Source Initiative site at:
;	https://opensource.org/licenses/gpl-3.0.html
*/

#ifndef MYCONVERTERS_H
#define MYCONVERTERS_H

#include "LibTypes.h"

#include <QRegularExpression>
#include <QString>

namespace NetAddrTools {

QString fromNetAddr(const NetAddr &netAddr);

NetAddr toNetAddr(QString value);

QRegularExpression ipRegEx();

}  // namespace NetAddrTools

#endif  // MYCONVERTERS_H
