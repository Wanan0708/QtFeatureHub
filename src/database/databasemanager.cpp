#include "databasemanager.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent)
{
}


DatabaseManager* DatabaseManager::instance()
{
    static DatabaseManager inst;
    return &inst;
}

bool DatabaseManager::connectToFile(const QString &filePath)
{
    if (m_db.isOpen()) return true;

    m_db = QSqlDatabase::addDatabase("QSQLITE", "QtFeatureHubConnection");
    m_db.setDatabaseName(filePath);

    if (!m_db.open()) {
        qWarning() << "Open SQLite failed:" << m_db.lastError().text();
        QSqlDatabase::removeDatabase("QtFeatureHubConnection");
        return false;
    }

    // Ensure table exists
    QSqlQuery q(m_db);
    const QString createTable = R"(
        CREATE TABLE IF NOT EXISTS persons (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            isClassmate INTEGER NOT NULL DEFAULT 0,
            name TEXT NOT NULL,
            age INTEGER NOT NULL
        );
    )";
    if (!q.exec(createTable)) {
        qWarning() << "Create table failed:" << q.lastError().text();
        m_db.close();
        QSqlDatabase::removeDatabase("QtFeatureHubConnection");
        return false;
    }

    return true;
}

bool DatabaseManager::isOpen() const
{
    return m_db.isOpen();
}

bool DatabaseManager::connectToHost(const QString &host, int port,
                                    const QString &user, const QString &password,
                                    const QString &dbName)
{
    // For backward compatibility, treat dbName as a file path for SQLite
    return connectToFile(dbName);
}

qint64 DatabaseManager::addPerson(bool isClassmate, const QString &name, int age)
{
    if (!m_db.isOpen()) return -1;
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO persons(isClassmate, name, age) VALUES(?, ?, ?)");
    q.addBindValue(isClassmate ? 1 : 0);
    q.addBindValue(name);
    q.addBindValue(age);
    if (!q.exec()) {
        qWarning() << "Insert failed:" << q.lastError().text();
        return -1;
    }
    QVariant lastId = q.lastInsertId();
    return lastId.isValid() ? lastId.toLongLong() : -1;
}

bool DatabaseManager::removePersonById(qint64 id)
{
    if (!m_db.isOpen()) return false;
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM persons WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec()) {
        qWarning() << "Delete failed:" << q.lastError().text();
        return false;
    }
    return q.numRowsAffected() > 0;
}

QList<QVariantMap> DatabaseManager::loadAll()
{
    QList<QVariantMap> list;
    if (!m_db.isOpen()) return list;
    QSqlQuery q(m_db);
    if (!q.exec("SELECT id, isClassmate, name, age FROM persons ORDER BY id")) {
        qWarning() << "Select failed:" << q.lastError().text();
        return list;
    }

    while (q.next()) {
        QVariantMap m;
        m["id"] = q.value(0).toLongLong();
        m["isClassmate"] = q.value(1).toInt();
        m["name"] = q.value(2).toString();
        m["age"] = q.value(3).toInt();
        list.append(m);
    }
    return list;
}
