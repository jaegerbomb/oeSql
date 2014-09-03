/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the demonstration applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "Browser.h"

#include <QtCore>
#include <QtWidgets>
#include <QtSql>
#include <iostream>


int main(int argc, char *argv[])
{
   QApplication app(argc, argv);

   QMainWindow mainWin;
   mainWin.setWindowTitle(QObject::tr("OpenEaagles Qt SQLite Parser and Browser"));
   //mainWin.setAttribute(Qt::WA_DeleteOnClose, true);
   //Lee - setting this attribute crashes the browser, but we need it to close the views - make this work.

   // Add our menu to the main browser
   Browser browser(&mainWin);
   mainWin.setCentralWidget(&browser);

   QMenu *fileMenu = mainWin.menuBar()->addMenu(QObject::tr("File"));
   fileMenu->addAction(QObject::tr("Add Database..."), &browser, SLOT(createConnection()));
   fileMenu->addAction(QObject::tr("Open Database..."), &browser, SLOT(openConnection()));
   fileMenu->addAction(QObject::tr("Close Database..."), &browser, SLOT(closeConnection()));
   fileMenu->addAction(QObject::tr("Close All &Dabases..."), &browser, SLOT(closeAllConnections()));
   fileMenu->addSeparator();
   fileMenu->addAction(QObject::tr("Quit"), &app, SLOT(quit()));

   QMenu *viewMenu = mainWin.menuBar()->addMenu(QObject::tr("View"));
   viewMenu->addAction(QObject::tr("All Objects and &Slots"), &browser, SLOT(viewObjectsAndSlots()));

   //Lee - deal with the WA_DeleteOnClose causing Widget to crash... find an elegant way to shut down the slot views as well.

   QMenu *helpMenu = mainWin.menuBar()->addMenu(QObject::tr("Help"));
   helpMenu->addAction(QObject::tr("About"), &browser, SLOT(about()));
   helpMenu->addAction(QObject::tr("About Qt"), qApp, SLOT(aboutQt()));

   QObject::connect(&browser, SIGNAL(statusMessage(QString)),
                  mainWin.statusBar(), SLOT(showMessage(QString)));


   mainWin.show();

   return app.exec();
}
