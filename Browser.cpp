#include "Browser.h"
#include "QSqlConnectionDialog.h"
#include "TreeModel.h"

#include <QtWidgets>
#include <QtSql>

#include <iostream>

Browser::Browser(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);

    table->addAction(insertRowAction);
    table->addAction(deleteRowAction);
    table->addAction(fieldStrategyAction);
    table->addAction(rowStrategyAction);
    table->addAction(manualStrategyAction);
    table->addAction(submitAction);
    table->addAction(revertAction);
    table->addAction(selectAction);
    slotView = 0;

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
   if (slotView != 0) {
      slotView->close();
   }
   QWidget::closeEvent(event);
}

void Browser::exec()
{
    QSqlQueryModel *model = new QSqlQueryModel(table);
    model->setQuery(QSqlQuery(sqlEdit->toPlainText(), connectionWidget->currentDatabase()));
    table->setModel(model);

    if (model->lastError().type() != QSqlError::NoError)
        emit statusMessage(model->lastError().text());
    else if (model->query().isSelect())
        emit statusMessage(tr("Query OK."));
    else
        emit statusMessage(tr("Query OK, number of affected rows: %1").arg(
                           model->query().numRowsAffected()));

    updateActions();
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

void Browser::addConnection()
{
    QSqlConnectionDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    // only Sqlite for now
    if (!dialog.databaseName().contains(".sqlite")) {
       QMessageBox::warning(this, tr("DB Mismatch"), tr("Database must be of type .sqlite"));
    }
    else {
      QSqlError err = addConnection("QSQLITE", dialog.databaseName());
       if (err.type() != QSqlError::NoError) {
           QMessageBox::warning(this, tr("Unable to open database"), tr("An error occurred while "
                                       "opening the connection: ") + err.text());
      }
      else {
         // we are not just reading, but creating the database
         if (dialog.createNewDatabase()) {
            // get the directory
            QString text = QFileDialog::getExistingDirectory(this, tr("Open OE Directory"),
                                                             "/home",
                                                             QFileDialog::ShowDirsOnly
                                                             | QFileDialog::DontResolveSymlinks);
            myParser.parse(text, dialog.databaseName());
         }
         connectionWidget->refresh();
      }
    }
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

    connect(table->selectionModel(), SIGNAL(currentRowChanged(QModelIndex,QModelIndex)),
            this, SLOT(currentChanged()));
    updateActions();
}

void Browser::showMetaData(const QString &t)
{
    QSqlRecord rec = connectionWidget->currentDatabase().record(t);
    QStandardItemModel *model = new QStandardItemModel(table);

    model->insertRows(0, rec.count());
    model->insertColumns(0, 7);

    model->setHeaderData(0, Qt::Horizontal, "Fieldname");
    model->setHeaderData(1, Qt::Horizontal, "Type");
    model->setHeaderData(2, Qt::Horizontal, "Length");
    model->setHeaderData(3, Qt::Horizontal, "Precision");
    model->setHeaderData(4, Qt::Horizontal, "Required");
    model->setHeaderData(5, Qt::Horizontal, "AutoValue");
    model->setHeaderData(6, Qt::Horizontal, "DefaultValue");


    for (int i = 0; i < rec.count(); ++i) {
        QSqlField fld = rec.field(i);
        model->setData(model->index(i, 0), fld.name());
        model->setData(model->index(i, 1), fld.typeID() == -1
                ? QString(QMetaType::typeName(fld.type()))
                : QString("%1 (%2)").arg(QMetaType::typeName(fld.type())).arg(fld.typeID()));
        model->setData(model->index(i, 2), fld.length());
        model->setData(model->index(i, 3), fld.precision());
        model->setData(model->index(i, 4), fld.requiredStatus() == -1 ? QVariant("?")
                : QVariant(bool(fld.requiredStatus())));
        model->setData(model->index(i, 5), fld.isAutoValue());
        model->setData(model->index(i, 6), fld.defaultValue());
    }

    table->setModel(model);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    updateActions();
}

void Browser::insertRow()
{
    QSqlRelationalTableModel *model = qobject_cast<QSqlRelationalTableModel *>(table->model());
    if (!model)
        return;

    QModelIndex insertIndex = table->currentIndex();
    int row = insertIndex.row() == -1 ? 0 : insertIndex.row();
    model->insertRow(row);
    insertIndex = model->index(row, 0);
    table->setCurrentIndex(insertIndex);
    table->edit(insertIndex);
}

void Browser::deleteRow()
{
    QSqlRelationalTableModel *model = qobject_cast<QSqlRelationalTableModel *>(table->model());
    if (!model)
        return;

    QModelIndexList currentSelection = table->selectionModel()->selectedIndexes();
    for (int i = 0; i < currentSelection.count(); ++i) {
        if (currentSelection.at(i).column() != 0)
            continue;
        model->removeRow(currentSelection.at(i).row());
    }

    updateActions();
}

void Browser::updateActions()
{
    QSqlRelationalTableModel * tm = qobject_cast<QSqlRelationalTableModel *>(table->model());
    bool enableIns = tm;
    bool enableDel = enableIns && table->currentIndex().isValid();

    insertRowAction->setEnabled(enableIns);
    deleteRowAction->setEnabled(enableDel);

    fieldStrategyAction->setEnabled(tm);
    rowStrategyAction->setEnabled(tm);
    manualStrategyAction->setEnabled(tm);
    submitAction->setEnabled(tm);
    revertAction->setEnabled(tm);
    selectAction->setEnabled(tm);

    if (tm) {
        QSqlTableModel::EditStrategy es = tm->editStrategy();
        fieldStrategyAction->setChecked(es == QSqlTableModel::OnFieldChange);
        rowStrategyAction->setChecked(es == QSqlTableModel::OnRowChange);
        manualStrategyAction->setChecked(es == QSqlTableModel::OnManualSubmit);
    }
}

void Browser::about()
{
    QMessageBox::about(this, tr("About"), tr("The SQL Browser demonstration "
        "shows how a data browser can be used to visualize the results of SQL"
                                             "statements on a live database"));
}

void Browser::on_fieldStrategyAction_triggered()
{
    QSqlRelationalTableModel * tm = qobject_cast<QSqlRelationalTableModel *>(table->model());
    if (tm)
        tm->setEditStrategy(QSqlTableModel::OnFieldChange);
}

void Browser::on_rowStrategyAction_triggered()
{
    QSqlRelationalTableModel * tm = qobject_cast<QSqlRelationalTableModel *>(table->model());
    if (tm)
        tm->setEditStrategy(QSqlTableModel::OnRowChange);
}

void Browser::on_manualStrategyAction_triggered()
{
    QSqlRelationalTableModel * tm = qobject_cast<QSqlRelationalTableModel *>(table->model());
    if (tm)
        tm->setEditStrategy(QSqlTableModel::OnManualSubmit);
}

void Browser::on_submitAction_triggered()
{
    QSqlRelationalTableModel * tm = qobject_cast<QSqlRelationalTableModel *>(table->model());
    if (tm)
        tm->submitAll();
}

void Browser::on_revertAction_triggered()
{
    QSqlRelationalTableModel * tm = qobject_cast<QSqlRelationalTableModel *>(table->model());
    if (tm)
        tm->revertAll();
}

void Browser::on_selectAction_triggered()
{
    QSqlRelationalTableModel * tm = qobject_cast<QSqlRelationalTableModel *>(table->model());
    if (tm)
        tm->select();
}

void Browser::on_viewButton_clicked()
{
   // let's build a view!
   QSqlDatabase db = connectionWidget->currentDatabase();
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
      //QTreeView* view = new QTreeView();
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
      if (slotView == 0) slotView = new QTreeView();
      slotView->setModel(model);
      slotView->resize(300, 300);
      slotView->show();
   }

}

