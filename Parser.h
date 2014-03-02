// top level parser that does all the parsing and putting of data in the database
#ifndef PARSER_H
#define PARSER_H

#include <QString>
#include <QProgressDialog>
#include <QList>
#include <QFile>
#include <QTextStream>

#include <QtWidgets>


class Parser : public QWidget
{
public:
   explicit Parser(QWidget* parent = 0);
   ~Parser();

   // parse and create a database from the dir into the dbName
   virtual void parse(QString dir, QString dbName);

private:
   void readDirectoriesForSource(QString path, QProgressDialog& progress, int& count);
   void readDirectoriesForInclude(QString path, QProgressDialog& progress, int& count, bool baseclassPass);
   void buildSlotTable(QFile &file);
   void buildClassTable(QFile &file, const bool baseclassPass);

   // checks and removes unecessary comments in the stream, returning each
   // line that isn't empty (with comments removed)
   QList<QString> removeComments(QTextStream& stream);

   static int getNextClassNum();
   static int getNextSlotNum();

   static int numClasses;     // number of classes in our class table
   static int numSlots;       // number of slots in our slot table
   QString databaseName;      // database name we are parsing into
};

inline int Parser::getNextClassNum()         { return numClasses++; }
inline int Parser::getNextSlotNum()          { return numSlots++; }

#endif // PARSER_H
