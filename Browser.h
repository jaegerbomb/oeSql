
#ifndef BROWSER_H
#define BROWSER_H

#include <QWidget>
#include <QSqlRelationalTableModel>
#include "./tmp/ui/ui_browserWidget.h"
#include "Parser.h"

class ConnectionWidget;
QT_FORWARD_DECLARE_CLASS(QTableView)
QT_FORWARD_DECLARE_CLASS(QPushButton)
QT_FORWARD_DECLARE_CLASS(QTextEdit)
QT_FORWARD_DECLARE_CLASS(QSqlError)

class Browser: public QWidget, private Ui::Browser
{
    Q_OBJECT
public:
    Browser(QWidget *parent = 0);
    virtual ~Browser();

    QSqlError addConnection(const QString &driver, const QString &dbName);

    void insertRow();
    void deleteRow();
    void updateActions();

public slots:
    void exec();
    void showTable(const QString &table);
    void showMetaData(const QString &table);
    void addConnection();
    void currentChanged() { updateActions(); }
    void about();

    void on_insertRowAction_triggered()
    { insertRow(); }
    void on_deleteRowAction_triggered()
    { deleteRow(); }
    void on_fieldStrategyAction_triggered();
    void on_rowStrategyAction_triggered();
    void on_manualStrategyAction_triggered();
    void on_submitAction_triggered();
    void on_revertAction_triggered();
    void on_selectAction_triggered();
    void on_connectionWidget_tableActivated(const QString &table)
    { showTable(table); }
    void on_connectionWidget_metaDataRequested(const QString &table)
    { showMetaData(table); }
    void on_submitButton_clicked()
    {
        exec();
        sqlEdit->setFocus();
    }
    void on_clearButton_clicked()
    {
        sqlEdit->clear();
        sqlEdit->setFocus();
    }
    void on_viewButton_clicked();

signals:
    void statusMessage(const QString &message);

protected:
   virtual void closeEvent(QCloseEvent* event);

private:
    // our parser
    Parser myParser;
    QTreeView* slotView;      // holds our slot view - if we have one
};

class CustomModel: public QSqlRelationalTableModel
{
    Q_OBJECT
public:
    explicit CustomModel(QObject *parent = 0, QSqlDatabase db = QSqlDatabase()):QSqlRelationalTableModel(parent, db) {}
    QVariant data(const QModelIndex &idx, int role) const
    {
        if (role == Qt::BackgroundRole && isDirty(idx))
            return QBrush(QColor(Qt::yellow));
        return QSqlRelationalTableModel::data(idx, role);
    }
};

#endif
