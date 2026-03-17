#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QList>
#include <QVariantMap>
#include <QSqlDatabase>

class DatabaseManager : public QObject
{
    Q_OBJECT
public:
    static DatabaseManager* instance();

    // Connect to MySQL server and ensure database/table exist
    // Connect to a SQLite database file (creates file if missing)
    bool connectToFile(const QString &filePath);

    // legacy: connect to host (kept for compatibility, will fallback to file)
    bool connectToHost(const QString &host, int port,
                       const QString &user, const QString &password,
                       const QString &dbName);

    bool isOpen() const;

    // CRUD helpers
    qint64 addPerson(bool isClassmate, const QString &name, int age);
    bool removePersonById(qint64 id);
    QList<QVariantMap> loadAll();

private:
    explicit DatabaseManager(QObject *parent = nullptr);
    QSqlDatabase m_db;
};

#endif // DATABASEMANAGER_H
