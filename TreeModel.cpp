/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
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
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
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

/*
    treemodel.cpp

    Provides a simple tree model to show how to create and use hierarchical
    models.
*/

#include "TreeItem.h"
#include "TreeModel.h"
#include <iostream>

#include <QStringList>

//! [0]
TreeModel::TreeModel(QObject *parent)
    : QAbstractItemModel(parent)
{
    QList<QVariant> rootData;
    rootData << "Classes";
    rootItem = new TreeItem(rootData);
//    setupModelData(data.split(QString(" ")), rootItem);
}
//! [0]

//! [1]
TreeModel::~TreeModel()
{
    delete rootItem;
}
//! [1]

//! [2]
int TreeModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return static_cast<TreeItem*>(parent.internalPointer())->columnCount();
    else
        return rootItem->columnCount();
}
//! [2]

//! [3]
QVariant TreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role != Qt::DisplayRole)
        return QVariant();

    TreeItem *item = static_cast<TreeItem*>(index.internalPointer());

    return item->data(index.column());
}
//! [3]

//! [4]
Qt::ItemFlags TreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;

    return QAbstractItemModel::flags(index);
}
//! [4]

//! [5]
QVariant TreeModel::headerData(int section, Qt::Orientation orientation,
                               int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return rootItem->data(section);

    return QVariant();
}
//! [5]

//! [6]
QModelIndex TreeModel::index(int row, int column, const QModelIndex &parent)
            const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    TreeItem *parentItem;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<TreeItem*>(parent.internalPointer());

    TreeItem *childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);
    else
        return QModelIndex();
}
//! [6]

//! [7]
QModelIndex TreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    TreeItem *childItem = static_cast<TreeItem*>(index.internalPointer());
    TreeItem *parentItem = childItem->parent();

    if (parentItem == rootItem)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}
//! [7]

//! [8]
int TreeModel::rowCount(const QModelIndex &parent) const
{
    TreeItem *parentItem;
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<TreeItem*>(parent.internalPointer());

    return parentItem->childCount();
}
//! [8]

TreeItem* TreeModel::addClass(const QString className)
{
   // get the last item in the tree
   // add a new parent to the root item - classes are top level
   TreeItem* newChild = new TreeItem(className, rootItem);
   rootItem->appendChild(newChild);
   return newChild;
}

void TreeModel::addSlot(const QString slotName, const QString slotType, TreeItem* parentClass)
{
   if (parentClass != rootItem) {
      TreeItem* newSlot = new TreeItem(slotName, parentClass);
      parentClass->appendChild(newSlot);
      // now parse through each slot type
      QStringList list = slotType.split(" ", QString::SkipEmptyParts);
      while (!list.isEmpty()) {
         QString name = list.takeFirst();
         TreeItem* newSlotType = new TreeItem(name, newSlot);
         newSlot->appendChild(newSlotType);
      }
   }
}

//void TreeModel::setupModelData(const QStringList &lines, TreeItem *parent)
//{
//    QList<TreeItem*> parents;
//    parents << parent;

//    int number = 0;

//    // first line is the object, rest are the slots!
//   QList<QVariant> columnData;
//   // the first number is the parent
//    while (number < lines.count()) {
//         columnData.clear();
//        if (!lines[number].isEmpty()) {
//           if (number == 0) {
//              std::cout << "PARENT LINE = " << lines[number].toStdString() << std::endl;
//           }
//           else std::cout << "      CHILD LINE = " << lines[number].toStdString() << std::endl;
//           columnData << lines[number];
////            // Read the column data from the rest of the line.
////            QStringList columnStrings = lineData.split("\t", QString::SkipEmptyParts);
////            QList<QVariant> columnData;
////            for (int column = 0; column < columnStrings.count(); ++column)
////                columnData << columnStrings[column];

////            if (position > indentations.last()) {
////                // The last child of the current parent is now the new parent
////                // unless the current parent has no children.
////                if (parents.last()->childCount() > 0) {
////                    parents << parents.last()->child(parents.last()->childCount()-1);
////                }
////            } else {
////                while (position < indentations.last() && parents.count() > 0) {
////                    parents.pop_back();
////                }
////            }

//            // Append a new item to the current parent's list of children.
//           parents.last()->appendChild(new TreeItem(columnData, parents.last()));
//        }

//        ++number;
//    }
//}
