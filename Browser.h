
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

public slots:
    void showTable(const QString &table);
    void openConnection();
    void createConnection();
    void closeConnection();
    void closeAllConnections();
    void about();

    void on_connectionWidget_tableActivated(const QString &table)
    { showTable(table); }
    void viewObjectsAndSlots();

signals:
    void statusMessage(const QString &message);

protected:
   virtual void closeEvent(QCloseEvent* event);

private:
    // our parser
    Parser myParser;
    QList<QTreeView*> slotViews;      // holds our summary slot views for each table
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
