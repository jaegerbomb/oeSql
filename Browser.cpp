#include "Browser.h"
#include "TreeModel.h"

#include <QtWidgets>
#include <QtSql>

#include <iostream>

Browser::Browser(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);

    slotViews.clear();

    if (QSqlDatabase::drivers().isEmpty())
        QMessageBox::information(this, tr("No database drivers found"),
                                 tr("This demo requires at least one Qt database driver. "
                                    "Please check the documentation how to build the "
                                    "Qt SQL plugins."));

    emit statusMessage(tr("Ready."));
}

Browser::~Browser()
{
}

void Browser::closeEvent(QCloseEvent* event)
{
   closeAllConnections();
   QWidget::closeEvent(event);
}

QSqlError Browser::addConnection(const QString &driver, const QString &dbName)
{
    QSqlDatabase db = QSqlDatabase::database(dbName);
    QSqlError err;
    if (!db.isOpen()) {
       db = QSqlDatabase::addDatabase(driver, dbName);
       db.setDatabaseName(dbName);
       if (!db.open()) {
           err = db.lastError();
           db = QSqlDatabase();
           QSqlDatabase::removeDatabase(dbName);
       }
    }
    else {
       db = QSqlDatabase();
      err.setType(QSqlError::UnknownError);
    }
    return err;
}

void Browser::openConnection()
{
   QStringList names = QFileDialog::getOpenFileNames(this, "Open Database", "/home", "*.sqlite");
   for (int i = 0; i < names.size(); i++) {
      QString name = names[i];
      QSqlError err = addConnection("QSQLITE", name);
      if (err.type() != QSqlError::NoError) {
           QMessageBox::warning(this, tr("Unable to open database"), tr("An error occurred while "
                                       "opening the connection: ") + err.text());
      }
   }
   connectionWidget->refresh();
}

void Browser::createConnection()
{
   QString dir = QFileDialog::getExistingDirectory(this, tr("Open OE Directory to Parse"),
                                                    "/home",
                                                    QFileDialog::ShowDirsOnly
                                                    | QFileDialog::DontResolveSymlinks);

   QString name = QFileDialog::getSaveFileName(this, "Create New Database Name", "/home", "*.sqlite");

   if (!dir.isEmpty() && !name.isEmpty()) {
      if (!name.endsWith(".sqlite")) name.append(".sqlite");
      QSqlError err = addConnection("QSQLITE", name);
      if (err.type() != QSqlError::NoError) {
         QMessageBox::warning(this, tr("Unable to open database"), tr("An error occurred while "
                                    "opening the connection: ") + err.text());
      }
      else {
         myParser.parse(dir, name);
         connectionWidget->refresh();
      }
   }
}

// closes the selected database
void Browser::closeConnection()
{
   QString dbName = connectionWidget->currDatabaseName();
   if (!dbName.isEmpty()) {
      int sIdx = dbName.lastIndexOf("/") + 1;
      QString temp = dbName.right(dbName.length() - sIdx);
      QSqlDatabase::removeDatabase(dbName);
      bool found = false;
      for (int i = 0; i < slotViews.size() && !found; i++) {
         if (slotViews[i]->windowTitle() == temp) {
            found = true;
            slotViews[i]->close();
            slotViews.removeAt(i);
         }
      }
      connectionWidget->refresh();
   }
}

void Browser::closeAllConnections()
{
   // close all views
   while (!slotViews.isEmpty()) {
      QTreeView* view = slotViews.first();
      view->close();
      slotViews.removeFirst();
   }

   QStringList dbNames = QSqlDatabase::connectionNames();
   for (int i = 0; i < dbNames.size(); i++) {
      QString dbName = dbNames[i];
      if (!dbName.isEmpty()) {
         int sIdx = dbName.lastIndexOf("/") + 1;
         QString temp = dbName.right(dbName.length() - sIdx);
         QSqlDatabase::removeDatabase(dbName);
      }
   }
   connectionWidget->refresh();
}



void Browser::showTable(const QString &t)
{
    CustomModel* model = new CustomModel(table, connectionWidget->currentDatabase());
    model->setEditStrategy(QSqlTableModel::OnRowChange);
    QString tableName = connectionWidget->currentDatabase().driver()->escapeIdentifier(t, QSqlDriver::TableName);
    model->setTable(tableName);
    // OE specific relationships
    if (tableName == "\"class\"") {
       // set our headers
       model->setHeaderData(0, Qt::Horizontal, "ID");
       model->setHeaderData(1, Qt::Horizontal, "CLASS NAME");
       model->setHeaderData(2, Qt::Horizontal, "FORM NAME");
       model->setHeaderData(3, Qt::Horizontal, "FILE NAME");
       model->setHeaderData(4, Qt::Horizontal, "BASECLASS");
       model->setJoinMode(QSqlRelationalTableModel::LeftJoin);
       model->setRelation(4, QSqlRelation("class", "id", "className"));
    }
    else if (tableName == "\"slotTable\"") {
       model->setHeaderData(0, Qt::Horizontal, "ID");
       model->setHeaderData(1, Qt::Horizontal, "SLOT NAME");
       model->setHeaderData(2, Qt::Horizontal, "PARENT OBJECT");
       model->setJoinMode(QSqlRelationalTableModel::LeftJoin);
       model->setRelation(2, QSqlRelation("class", "id", "className"));
    }
    else if (tableName == "\"slotObjTable\"") {
       model->setHeaderData(0, Qt::Horizontal, "SLOT ID");
       model->setHeaderData(1, Qt::Horizontal, "OBJECT TYPE");
       model->setJoinMode(QSqlRelationalTableModel::LeftJoin);
       model->setRelation(0, QSqlRelation("slotTable", "slotId", "slotName"));
       model->setRelation(1, QSqlRelation("class", "id", "className"));
    }
    model->select();
    if (model->lastError().type() != QSqlError::NoError)
        emit statusMessage(model->lastError().text());
    table->setModel(model);
    table->setEditTriggers(QAbstractItemView::DoubleClicked|QAbstractItemView::EditKeyPressed);
}



void Browser::about()
{
    QMessageBox::about(this, tr("About"), tr("Browser to parse and view OpenEaagles files using Sqlite."));
}


void Browser::viewObjectsAndSlots()
{
   // get all the databases, and build views for each
   QStringList strings = QSqlDatabase::connectionNames();
   for (int i = 0; i < strings.size(); i++) {
      QSqlDatabase db = QSqlDatabase::database(strings[i]);
      if (db.isOpen()) {

         // build a query
         QSqlQuery* query = new QSqlQuery(db);


         // grab the first id of the class
         query->exec("SELECT id, className from class");
         QSqlQuery* slotQuery = new QSqlQuery(db);
         QSqlQuery* slotTypeQuery = new QSqlQuery(db);
         QSqlQuery* tQuery = new QSqlQuery(db);
         QString final;
         QString finalClass;
         QString finalClassTypes;
         TreeModel* model = new TreeModel();
         while (query->next()) {
            finalClass.clear();
            int classId = query->value(0).toInt();
            finalClass = query->value(1).toString();
            // now get the files from it
            TreeItem* classItem = 0;
            if (!finalClass.isEmpty()) {
               //std::cout << "CLASS AND ID = " << finalClass.toStdString() << ", " << classId << std::endl;
               classItem = model->addClass(finalClass);
               QString queryString = QString("SELECT slotName, slotId FROM slotTable WHERE parentId = %1;").arg(classId);
               slotQuery->exec(queryString);
               while (slotQuery->next() && classItem != 0) {
                  final.clear();
                  finalClassTypes.clear();
                  // each slot may have multiple types... let's query them
                  final = slotQuery->value(0).toString();
                  int slotId = slotQuery->value(1).toInt();
                  //std::cout << " SLOT and ID = " << final.toStdString() << ", " << slotId << std::endl;
                  // we have to go through and find all the type's of objects the the slot query can handle... let's try it.
                  QString slotTypeString = QString("SELECT objId  FROM slotObjTable WHERE slotId = %1;").arg(slotId);
                  slotTypeQuery->exec(slotTypeString);
                  while(slotTypeQuery->next()) {
                     int slotObjId = slotTypeQuery->value(0).toInt();
                     slotTypeString = QString("SELECT className FROM class WHERE id = %1;").arg(slotObjId);
                     tQuery->exec(slotTypeString);
                     tQuery->first();
                     finalClassTypes.append(tQuery->value(0).toString() + " ");
                     //std::cout << "CLASS NAME OF OBJECT TYPE " << slotId << " = " << tQuery->value(0).toString().toStdString() << std::endl;
                     //std::cout << "SLOT NUM && OBJ = " << slotNum << slotObj << std::endl;
                  }
                  //std::cout << "FINAL SLOT TYPES = " << finalClassTypes.toStdString() << std::endl;
                  if (!final.isEmpty()) {
                     model->addSlot(final, finalClassTypes, classItem);
                  }
               }
            }

         }
         QString temp = strings[i];
         // we only want the file name!
         int sIdx = temp.lastIndexOf("/") + 1;
         temp = temp.right(temp.length() - sIdx);
         bool found = false;
         for (int i = 0; i < slotViews.size() && !found; i++) {
            if (slotViews[i]->windowTitle() == temp) {
               found = true;
               slotViews[i]->setModel(model);
               slotViews[i]->show();
               slotViews[i]->activateWindow();
            }
         }
         if (!found) {
            QTreeView* sv = new QTreeView();
            sv->setWindowTitle(temp);
            sv->setModel(model);
            sv->resize(300, 300);
            sv->show();
            slotViews << sv;
         }
      }
   }
}

