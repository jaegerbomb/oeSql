#include "Parser.h"
#include <QMessageBox>
#include <QSqlDatabase>
#include <QString>
#include <QSqlQuery>
#include <QApplication>
#include <QDir>

#include <iostream>

int Parser::numClasses = 0;
int Parser::numSlots = 0;

Parser::Parser(QWidget *parent)
   : QWidget(parent)
{
   //db = 0;

   if (QSqlDatabase::drivers().isEmpty())
        QMessageBox::information(this, tr("No database drivers found"),
                                 tr("This demo requires at least one Qt database driver. "
                                    "Please check the documentation how to build the "
                                    "Qt SQL plugins."));

}

Parser::~Parser()
{

}

void Parser::parse(QString dir, QString dbName)
{
   // make sure there is a database
   QSqlDatabase db = QSqlDatabase::database(dbName);

   if (db.isOpen()) {
      databaseName = dbName;
      bool noTables = (db.tables().count() == 0);
      QSqlQuery* query = new QSqlQuery(db);
      if (noTables) {
         // new tables
         // Class table
         // ID: integer
         // Class name: string
         // Form name: Factory name (.epp) of the class
         // File name: originating file name
         // Baseclass: integer to another object (if this object is derived)
         query->exec("create table class (id integer, className varchar(50), formName varchar(50), fileName varchar(50), baseClass integer)");

         // new slot table
         // Slot Table
         // slotId: integer
         // slotName: name of the slot
         // parentId: object id of the class in which this slot belongs to
         query->exec("create table slotTable (slotId integer primary key, slotName varchar(50), parentId integer)");

         // and the slot to object table
         // Slot Object Table
         // objId: id of the object type we are, reference class.id
         // slotId: id of the slot we are representing, referencing slotTable.slotId
         query->exec("create table slotObjTable (slotId integer, objId integer)");
      }
      else {
         query->exec("DELETE FROM class");
         query->exec("DELETE from slotTable");
         query->exec("DELETE from slotObjTable");
      }


      // make sure to reset our index number
      Parser::numClasses = 0;

      QProgressDialog progressDialog(this);
      progressDialog.autoReset();
      progressDialog.setModal(true);
      progressDialog.setWindowTitle("Parsing Classes");
      progressDialog.setMaximum(0);
      progressDialog.show();

      int count = 0;


      // we have to do this two times.. one for building classes, the other for the baseclasses
      readDirectoriesForInclude(dir, progressDialog, count, false);
      readDirectoriesForInclude(dir, progressDialog, count, true);

      if (progressDialog.wasCanceled()) {
         // clear our database
         query->exec("DELETE FROM class");
         query->exec("DELETE from slotTable");
         query->exec("DELETE from slotObjTable");
         QMessageBox::information(0, "PARSING STOPPED", "Parsing was cancelled by user");
         return;
      }
      progressDialog.close();

      QProgressDialog slotDialog(this);
      slotDialog.autoReset();
      slotDialog.setModal(true);
      slotDialog.setWindowTitle("Parsing Slots");

      slotDialog.setMaximum(0);
      slotDialog.show();
      count = 0;

      // we have to do this two times.. one for building classes, the other for the baseclasses
      readDirectoriesForSource(dir, slotDialog, count);


      if (slotDialog.wasCanceled()) {
         // clear our database
         query->exec("DELETE FROM class");
         query->exec("DELETE from slotTable");
         query->exec("DELETE from slotObjTable");
         QMessageBox::information(this, "PARSING STOPPED", "Parsing was cancelled by user");
         return;
      }
      slotDialog.close();

      QString numParsed = QString("Files parsed: %1").arg(count);
      QMessageBox::information(this, "PARSING COMPLETE", numParsed);
   }

}


// go through and read all the directories, looking for source files to parse
void Parser::readDirectoriesForSource(QString path, QProgressDialog &progress, int &count)
{
   if (progress.wasCanceled()) {
      return;
   }
   progress.setValue(progress.value() + 5);
   qApp->processEvents();

   QDir dir(path);
   QFileInfoList dirs = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs);
   QStringList filters;
   filters << "*.cpp";
   // First, look for .cpp files to parse
   QStringList fileList = dir.entryList(filters, QDir::Files);
   QFile file;
   QString finalString;
   // parse the files, and add the contents to the database
   for (int i = 0; i < fileList.size(); i++) {
      progress.setValue(progress.value() + 5);
      qApp->processEvents();
      finalString = dir.absolutePath();
      finalString.append("/");
      finalString.append(fileList[i]);
      file.setFileName(finalString);
      file.open(QFile::ReadOnly);
      count++;
      buildSlotTable(file);
      file.close();
   }
   for (int i = 0; i < dirs.size(); i++) {
      QString newPath = dirs[i].absoluteFilePath();
      readDirectoriesForSource(newPath, progress, count);
   }
}

// go through and read all the directories, looking for include files to parse
void Parser::readDirectoriesForInclude(QString path, QProgressDialog &progress, int& count, bool baseclassPass)
{
   if (progress.wasCanceled()) {
      return;
   }
   progress.setValue(progress.value() + 5);
   qApp->processEvents();

   //std::cout << "READING DIRECTORY = " << path.toStdString() << std::endl;

   QDir dir(path);
   QFileInfoList dirs = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs);
   QStringList filters;
   filters << "*.h";
   // First, look for .cpp files to parse
   QStringList fileList = dir.entryList(filters, QDir::Files);
   QFile file;
   QString finalString;
   // parse the files, and add the contents to the database
   for (int i = 0; i < fileList.size(); i++) {
      progress.setValue(progress.value() + 5);
      qApp->processEvents();
      finalString = dir.absolutePath();
      finalString.append("/");
      finalString.append(fileList[i]);
      file.setFileName(finalString);
      file.open(QFile::ReadOnly);
      count++;
      buildClassTable(file, baseclassPass);
      file.close();
   }
   for (int i = 0; i < dirs.size(); i++) {
      QString newPath = dirs[i].absoluteFilePath();
      readDirectoriesForInclude(newPath, progress, count, baseclassPass);
   }
}


// This function is looking at a header file, and will build the class using the explicit name.  It will also
// check for a baseclass, and will use that explicit name as well
// Example:
// Class:                        BaseClass:
// Eaagles::BasicGL::Graphic     Eaagles::Basic::Object
// All classes will have fully qualified namespace names.  Their formNames will be the 'shorthand' version of this (and what
// is used in the parser)
// If the baseclassPass flag is enabled, it means we have already built the classes, and are now going back through to update the base
// classes (this has to be done this way because we can't guarantee the order that the classes are built)
void Parser::buildClassTable(QFile &file, const bool baseclassPass)
{
   // there are certain files that we can simply throw out... such as files that are xxx.xx.h, because those aren't openEaagles files.
   if (file.fileName().count(".") > 1) return;

   QTextStream stream(&file);
   QList<QString> strings = removeComments(stream);
   QList<QString> namespaces;

   // have to find the namespaces, ignoring forward declarations
   // if we hit the class definition BEFORE we get closing brackets for namespaces, we keep the namespaces
   bool classStarted = false;
   bool hasNameSpace = false;

   // we have to validate the file and make sure it has a class definition in it!
   for (int i = 0; i < strings.size() && !classStarted; i++) {
      if (strings[i].contains("class") && !strings[i].contains(";")) {
         classStarted = true;
      }
   }

   if (classStarted) {
      classStarted = false;
      QString queryString;
      // make sure there is a database
      QSqlDatabase db = QSqlDatabase::database(databaseName);
      QSqlQuery* query = new QSqlQuery(db);
      // for counting braces before the class starts
      int numBraces = 0;
      for (int i = 0; i < strings.size(); i++) {
         // possible candidate, let's see what happen
         if (strings[i].contains("namespace")) {
            // check to see if it's forward decs in the same line
            if (!strings[i].contains("}")) {
               hasNameSpace = true;
               // we have to edit the namespace to be in the right format
               strings[i].replace("namespace", "");
               strings[i].replace(" ", "");
               strings[i].replace("{", "");
               strings[i].append("::");
               namespaces.push_back(strings[i]);
            }
         }
         // a function before the class... handle it.
         else if (hasNameSpace && strings[i].contains("{") && !strings[i].contains("class")) {
            numBraces++;
         }
         else if (hasNameSpace && strings[i].contains("}")) {
            if (numBraces > 0) numBraces--;
            else if (!classStarted) {
               // error handling - something went wrong
               if (namespaces.isEmpty()) {
                  std::cout << "NO!" << std::endl;
                  std::cout << "FILE NAME = " << file.fileName().toStdString() << std::endl;
                  exit(1);
               }
               else namespaces.pop_back();
            }
         }
         // class definition has started
         else if (hasNameSpace && strings[i].contains("class") && !strings[i].contains(";")) {
            // at this point you have a string that is either
            // class XXXX : public XXXX {
            // or class XXXX {
            classStarted = true;
            // let's print the class name
            int idx = strings[i].indexOf("class");
            // derived class... let's look at it!
            if (strings[i].contains(":")) {
               int dIdx = strings[i].indexOf(":");
               // first things first, let's grab the class name
               QString cString = strings[i].left(dIdx);
               cString.replace("class", "");
               cString.replace(" ", "");
               // now prepend the class names
               for (int j = namespaces.size()-1; j >= 0; j--) {
                  cString.prepend(namespaces[j]);
               }

               if (baseclassPass) {
                  // ---
                  // we need to determine the baseclass information, and gleam it's fully qualified name.  We are going to do this as such:
                  // Parse the name - and then look for the colons (::) indicating that is a qualified name.  Then go back through our
                  // namespaces and see if they match.  If they do, we copy the orignating namespace up to the namespace that matches the object,
                  // then append the rest.
                  // For Example
                  // namespace Eaagles {
                  // namespace XXXXX {
                  //    class MyClass : public YYYYY::AClass
                  //
                  // 1st step - check if there is an object called YYYYY::AClass that matches EXACLY
                  // if not - look for Eaagles::XXXXX::YYYYY::AClass
                  // if not - look for Eaagles::YYYYY::AClass
                  // This exhaustively searches the namespace to find the object
                  // ----
                  // remove and public or private information
                  QString dString = strings[i];
                  dString.replace("public", "");
                  dString.replace("private", "");
                  dString.replace("protected", "");
                  dIdx = dString.indexOf(":");
                  int end = dString.indexOf("{");
                  dString = dString.mid(dIdx + 1, (end - dIdx) - 1 );
                  dString.replace(" ", "");
                  // qualified name!!!
                  if (dString.contains("::")) {
                     // ok, let's start by seeing if JUST this name exists
                     //std::cout << "TRYING TO FIND EXPLICIT BASECLASS " << dString.toStdString() << " FROM CLASS " << cString.toStdString() << std::endl;
                     bool ok = false;
                     int val = 0;
                     //std::cout << "LOOKING FOR " << dString.toStdString() << std::endl;
                     // first step, just query it like it is.
                     queryString = QString("SELECT id from class WHERE className='" + dString + "'");
                     query->exec(queryString);
                     ok = query->first();
                     if (ok) {
                        // did we find it?
                        val = query->value(0).toInt();
                     }
                     else {
                        // it didn't find it as is... let's start with the first explicit namespace, and see if we already have it
                        int idx = dString.indexOf("::");
                        // copy it
                        QString tString = dString.left(idx+2);
                        // now we have the first namespace... let's search our namespaces for a match
                        bool found = false;
                        for (int j = namespaces.size() - 1 && !found; j > 0; j--) {
                           if (namespaces[j] == tString) {
                              found = true;
                              // we found the same namespace... remove it!
                              dString.replace(tString, "");
                              // now just search for it as is again!
                              queryString = QString("SELECT id from class WHERE className='" + dString + "'");
                              query->exec(queryString);
                              ok = query->first();
                              if (ok) {
                                 // did we find it?
                                 val = query->value(0).toInt();
                              }
                           }
                        }

                        // ok, it wasn't found, we have to start prepending our namespaces to it until we find it (starting at the highest)
                        if (!found) {
                           QString nString;
                           for(int j = 0; j < namespaces.size() && !ok; j++) {
                              nString.append(namespaces[j]);
                              //std::cout << "LOOKING FOR " << (nString + dString).toStdString() << std::endl;
                              // first step, just query it like it is.
                              queryString = QString("SELECT id from class WHERE className='" + nString + dString + "'");
                              query->exec(queryString);
                              ok = query->first();
                              if (ok) {
                                 // did we find it?
                                 val = query->value(0).toInt();
                              }
                           }
                        }
                     }

                     if (ok) {
                        //std::cout << "FOUND BASECLASS!" << std::endl;
                        QString bqString = QString("UPDATE class SET baseclass=%1").arg(val);
                        bqString.append(" WHERE className = '" + cString + "'");
                        query->exec(bqString);
                     }
                  }
                  else {
                     // prepend our namespaces
                     for (int j = namespaces.size()-1; j >= 0; j--) {
                        dString.prepend(namespaces[j]);
                     }

                     //std::cout << "TRYING TO FIND BASECLASS " << dString.toStdString() << " FROM CLASS " << cString.toStdString() << std::endl;
                     // Find our baseclass class in the records
                     queryString = QString("SELECT id from class WHERE className='" + dString + "'");
                     query->exec(queryString);
                     if (query->isActive()) {
                        bool ok = query->first();
                        if (ok) {
                           int val = query->value(0).toInt();
                           QString bqString = QString("UPDATE class SET baseclass=%1").arg(val);
                           bqString.append(" WHERE className = '" + cString + "'");
                           query->exec(bqString);
                           //std::cout << "BASECLASS CLASS WAS FOUND at position " << val << std::endl;
                        }
                     }
                  }
               }
               else {
                  //std::cout << "ADDING CLASS = " << cString.toStdString() << std::endl;
                  // build the query
                  // ID - get a new one
                  int nextRow = getNextClassNum();
                  queryString = QString("insert into class values(%1").arg(nextRow);
                  queryString.append(", '" + cString + "'");
                  queryString.append(", NULL, '" + file.fileName() + "', NULL)");
                  query->exec(queryString);
               }
            }
            else if (!baseclassPass) {
               int end = strings[i].indexOf("{");
               QString temp = strings[i].mid(idx, end - idx);
               // remove the "class"
               temp.replace("class", "");
               temp.replace(" ", "");
               // now prepend the class names
               for (int j = namespaces.size() - 1; j >= 0; j--) {
                  temp.prepend(namespaces[j]);
               }
               //std::cout << "NON DERIVED CLASS NAME = " << temp.toStdString() << std::endl;
               // ID - get a new one
               int nextRow = getNextClassNum();
               queryString = QString("insert into class values(%1").arg(nextRow);
               queryString.append(", '" + temp + "'");
               queryString.append(", NULL, '" + file.fileName() + "', NULL)");
               query->exec(queryString);
            }
         }
      }

//      std::cout << "FILE NAME = " << file.fileName().toStdString() << std::endl;
//      for (int i = 0; i < namespaces.size(); i++) {
//         std::cout << "NAMESPACES = " << namespaces[i].toStdString() << std::endl;
//      }
//      std::cout << std::endl << std::endl;
   }
   //else std::cout << "NO class definition in File = " << file.fileName().toStdString() << std::endl;

}

void Parser::buildSlotTable(QFile& file)
{
   // we are going to parse the file twice.. once for class information and then
   // again for slot information
   if (file.isOpen()) {
      // first thing we do is search for a class name
      QTextStream stream(&file);
      // parse each into a line
      // make sure there is a database
      QSqlDatabase db = QSqlDatabase::database(databaseName);
      // Sql stuff
      QSqlQuery* query = new QSqlQuery(db);
      QString queryString;
      // comment stripped string from file
      QList<QString> strings = removeComments(stream);
      // We may have situations where multiple classes are defined in a single file... in which case we will
      // have more than one class name.
      // name(s) of the classes we belong to
      QList<QString> classNames;
      // Index maps of that given class (this lines up with classnames)
      QList< QMap<int, int> > tIdxToSlotId;

      // our top level namespaces (so we can make fully qualified names)
      QList<QString> namespaces;

      // parse our strings
      for (int i = 0; i < strings.size(); i++) {
         // look for namespaces first, (because they will be).
         // We ignore using namespace commands - this isn't OE design and can be tough to parse (for example, using namespace std)
         if (strings[i].contains("namespace") && !strings[i].contains("using")) {
            int idx = strings[i].indexOf("namespace");
            int fIndex = strings[i].indexOf("{");
            idx += 9;
            QString temp = strings[i].mid(idx, (fIndex - idx) - 1);
            // now of course remove any spaces
            temp.replace(" ", "");
            temp.append("::");
            namespaces.push_front(temp);
         }
         else if (strings[i].contains("IMPLEMENT_")) {
            // grab the class name
            int startJ = strings[i].indexOf("(", 0);
            int lastJ = strings[i].indexOf(",", 0);
            QString className = strings[i].mid(startJ+1, (lastJ - startJ)-1);
            for (int x = 0; x < namespaces.size(); x++) {
               // append the namespaces!
               className.prepend(namespaces[x]);
            }
            // now the form name
            startJ = strings[i].indexOf("\"", 0);
            lastJ = strings[i].indexOf("\"", startJ+1) ;
            QString formName = strings[i].mid(startJ+1, (lastJ - startJ)-1);
            // update the formname for this object
            QString bqString = QString("UPDATE class SET formname='" + formName + "' WHERE className = '" + className + "'");
            query->exec(bqString);
         }
         else if (strings[i].contains("BEGIN_SLOTTABLE(")) {
            // now let's update the slots
            // we need to find out which class these slots belong to
            int startIdx = strings[i].indexOf("(", 0);
            int endIdx = strings[i].indexOf(")", startIdx+1);
            QString cbt = strings[i].mid(startIdx+1, (endIdx-startIdx) - 1);
            // don't add a class name until the slottable is there (which it is!)
            classNames << cbt;
            // fully qualify the class name
            for (int x = 0; x < namespaces.size(); x++) {
               // append the namespaces!
               cbt.prepend(namespaces[x]);
            }

            // now that we know the class name... let's add the slots.  But first we have to get the class names id.
            int classId = -1;
            queryString = "SELECT className, id from class WHERE className='" + cbt + "'";
            query->exec(queryString);
            if (query->isActive()) {
               bool ok = query->first();
               if (ok) classId = query->value(1).toInt();
            }
            if (classId != -1) {
               // increment our string
               i++;
               int startIdx = 0;
               QMap<int, int> tempIdx;
               while (i < strings.size() && !strings[i].contains("END_SLOTTABLE(")) {
                  int slotStart = strings[i].indexOf("\"", 0);
                  int slotEnd = strings[i].indexOf("\"", slotStart+1);
                  // there may be multiple slots on a single line, comma delimited
                  while (slotStart != -1 && slotEnd > slotStart) {
                     QString slotName = strings[i].mid(slotStart+1, (slotEnd-slotStart) - 1);
                     //std::cout << "SLOT NAME = " << slotName.toStdString() << std::endl;
                     int nextSlot = getNextSlotNum();
                     //std::cout << "SLOT INDEX = " << nextSlot << std::endl;
                     tempIdx.insert(startIdx++, nextSlot);
                     queryString = QString("insert into slotTable values(%1").arg(nextSlot);
                     QString other = QString(", '" + slotName + "', " + "%1)").arg(classId);
                     queryString += other;
                     query->exec(queryString);
                     slotStart = strings[i].indexOf("\"", slotEnd+1);
                     slotEnd  = strings[i].indexOf("\"", slotStart+1);
                  }
                  // increment our string
                  i++;
               }
               tIdxToSlotId << tempIdx;
            }
         }
         // now it's time for slot index mapping to the objects they will accept
         else if (strings[i].contains("BEGIN_SLOT_MAP(")) {
            // find the name of the class in which we are beginning the map for
            int startIdx = strings[i].indexOf("(", 0);
            int endIdx = strings[i].indexOf(")", startIdx+1);
            QString cbt = strings[i].mid(startIdx+1, (endIdx-startIdx) - 1);
            // find the position in the list in which this guy is
            bool found = false;
            int idxPos = -1;
            for (int x = 0; x < classNames.size() && !found; x++) {
               if (cbt == classNames[x]) {
                  idxPos = x;
                  found = true;
               }
            }

            // increment to the next string
            i++;
            while (i < strings.size() && !strings[i].contains("END_SLOT_MAP(")) {
               strings[i].replace(" ", "");
               // remove any whitespace
               int slotStart = strings[i].indexOf("(", 0);
               int slotEnd = strings[i].indexOf(")", slotStart+1);
               if (slotStart != -1 && slotEnd > slotStart) {
                  // find our first comma
                  slotEnd = strings[i].indexOf(",", slotStart+1);

                  int slotId = strings[i].mid(slotStart+1, (slotEnd-slotStart) - 1).toInt();
                  // map this to the actual position in the table
                  if (slotId > 0 && idxPos != -1) {
                     int actSlotId = tIdxToSlotId[idxPos].value(slotId-1);
                     // now let's find the object type.
                     slotStart = strings[i].indexOf(",", slotEnd+1);
                     slotEnd = strings[i].indexOf(")", slotStart+1);
                     QString objTypeName = strings[i].mid(slotStart+1, (slotEnd - slotStart) - 1);
                     // qualified name!!!
                     if (objTypeName.contains("::")) {
                        // ok, let's start by seeing if JUST this name exists
                        bool ok = false;
                        int val = 0;
                        //std::cout << "LOOKING FOR " << objTypeName.toStdString() << std::endl;
                        // first step, just query it like it is.
                        queryString = QString("SELECT id from class WHERE className='" + objTypeName + "'");
                        query->exec(queryString);
                        ok = query->first();
                        if (ok) {
                           // did we find it?
                           val = query->value(0).toInt();
                        }
                        else {
                           // it didn't find it as is... let's start with the first explicit namespace
                           int idx = objTypeName.indexOf("::");
                           // copy it
                           QString tString = objTypeName.left(idx+2);
                           // now we have the first namespace... let's search our namespaces for a match
                           bool found = false;
                           for (int j = namespaces.size() - 1; j > 0; j--) {
                              if (namespaces[j] == tString) {
                                 found = true;
                              }
                           }

                           // ok, it wasn't found, we have to start prepending our namespaces to it until we find it (starting at the highest)
                           if (!found) {
                              QString nString;
                              for(int j = namespaces.size() - 1; j > 0 && !ok; j--) {
                                 nString.append(namespaces[j]);
                                 //std::cout << "LOOKING FOR " << (nString + dString).toStobjTypeName() << std::endl;
                                 // first step, just query it like it is.
                                 queryString = QString("SELECT id from class WHERE className='" + nString + objTypeName + "'");
                                 query->exec(queryString);
                                 ok = query->first();
                                 if (ok) {
                                    // did we find it?
                                    val = query->value(0).toInt();
                                 }
                              }
                           }
                        }

                        if (ok) {
                           queryString = QString("insert into slotObjTable values(%1").arg(actSlotId);
                           QString anotherString = QString(", %1)").arg(val);
                           queryString.append(anotherString);
                           query->exec(queryString);
                        }
                     }
                     else {
                        // just use the fully qualified name
                        for (int x = 0; x < namespaces.size(); x++) {
                           objTypeName.prepend(namespaces[x]);
                        }
                        queryString = QString("SELECT id from class WHERE className='" + objTypeName + "'");
                        query->exec(queryString);
                        bool ok = query->first();
                        int val = 0;
                        if (ok) {
                           // did we find it?
                           val = query->value(0).toInt();
                           queryString = QString("insert into slotObjTable values(%1").arg(actSlotId);
                           QString anotherString = QString(", %1)").arg(val);
                           queryString.append(anotherString);
                           query->exec(queryString);
                        }

                     }

                  }
               }
               // increment our string
               i++;
            }
         }
      }
   }

}

// checks the string for any comment lines, and if the line is ALL comments, return true.
// If there is any valid information in it.. the information is extracted in the string
QList<QString> Parser::removeComments(QTextStream& stream)
{
   QList<QString> strings;
   QString line = stream.readLine();
   while (!stream.atEnd()) {
      // single line comment
      if (line.contains("//")) {
         int indexOfComment = line.indexOf("//");
         line.chop(line.size() - indexOfComment);
         // the line still has something in it...let's test for whitespace only
         if (!line.isEmpty()) {
            QString temp = line;
            temp.replace(" ", "");
            if (temp.isEmpty()) line.clear();
         }
      }
      // beginning of a multi-line comment
      if (line.contains("/*")) {
         // if the line is a multi-line on a single line
         if (line.contains("*/")) {
            int sIndex = line.indexOf("/*");
            int eIndex = line.indexOf("*/");
            // remove it
            line.remove(sIndex, (eIndex + 2) - sIndex);
            // if there is STILL something left.. make sure it's not spaces
            if (!line.isEmpty()) {
               QString temp = line;
               temp.replace(" ", "");
               if (temp.isEmpty()) line.clear();
            }
         }
         // multi-line comment
         else {
            // keep reading until we find the closing comment character sequence
            while (!line.contains("*/") && !stream.atEnd()) {
               line = stream.readLine();
            }
            // we have ended the comment.. let's chop it and make sure there is nothing left
            int idx = line.indexOf("*/");
            if (idx != -1) {
               line.remove(0, idx + 2);
               QString temp = line;
               temp.replace(" ", "");
               if (temp.isEmpty()) line.clear();
            }
         }

      }
      if (!line.isEmpty()) strings << line;
      line = stream.readLine();
   }
   return strings;
}


