#include "database.h"
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
[[noreturn]] void fail(const QString &message) { throw std::runtime_error(message.toUtf8().constData()); }
void execute(QSqlQuery &query) {
    if (!query.exec()) fail(query.lastError().text());
}
void sql(QSqlDatabase db, const QString &statement) {
    QSqlQuery query(db);
    if (!query.exec(statement)) fail(query.lastError().text());
}
class Transaction {
public:
    explicit Transaction(QSqlDatabase db) : db_(db) {
        if (!db_.transaction()) fail(db_.lastError().text());
    }
    ~Transaction() { if (!committed_) db_.rollback(); }
    void commit() {
        if (!db_.commit()) fail(db_.lastError().text());
        committed_ = true;
    }
private:
    QSqlDatabase db_;
    bool committed_ = false;
};
}

QString shapeTypeName(ShapeType type) {
    switch (type) {
    case ShapeType::Rectangle: return QStringLiteral("Прямоугольник");
    case ShapeType::Ellipse: return QStringLiteral("Эллипс");
    case ShapeType::Polygon: return QStringLiteral("Многоугольник");
    }
    return {};
}

Database::Database(const QString &path) : connectionName_(QUuid::createUuid().toString()) {
    if (path != ":memory:" && !QDir().mkpath(QFileInfo(path).absolutePath()))
        fail(QStringLiteral("Не удалось создать папку базы данных."));
    db_ = QSqlDatabase::addDatabase("QSQLITE", connectionName_);
    db_.setDatabaseName(path);
    db_.setConnectOptions("QSQLITE_BUSY_TIMEOUT=5000");
    try {
        if (!db_.open()) fail(db_.lastError().text());
        sql(db_, "PRAGMA foreign_keys = ON");
        initialize();
    } catch (...) {
        db_.close();
        db_ = {};
        QSqlDatabase::removeDatabase(connectionName_);
        throw;
    }
}

Database::~Database() {
    db_.close();
    db_ = {};
    QSqlDatabase::removeDatabase(connectionName_);
}

void Database::initialize() {
    Transaction transaction(db_);
    sql(db_, R"SQL(CREATE TABLE IF NOT EXISTS shapes (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        type INTEGER NOT NULL CHECK(type BETWEEN 0 AND 2),
        x REAL NOT NULL, y REAL NOT NULL,
        width REAL NOT NULL CHECK(width BETWEEN 20 AND 500),
        height REAL NOT NULL CHECK(height BETWEEN 20 AND 500),
        vertices INTEGER NOT NULL DEFAULT 5 CHECK(vertices BETWEEN 3 AND 12)))SQL");
    sql(db_, R"SQL(CREATE TABLE IF NOT EXISTS connections (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        first_id INTEGER NOT NULL REFERENCES shapes(id) ON DELETE CASCADE,
        second_id INTEGER NOT NULL REFERENCES shapes(id) ON DELETE CASCADE,
        CHECK(first_id < second_id), UNIQUE(first_id, second_id)))SQL");
    sql(db_, "CREATE INDEX IF NOT EXISTS connections_second ON connections(second_id)");
    sql(db_, R"SQL(CREATE TABLE IF NOT EXISTS shape_type_stats (
        type INTEGER PRIMARY KEY CHECK(type BETWEEN 0 AND 2),
        count INTEGER NOT NULL DEFAULT 0 CHECK(count >= 0)))SQL");
    sql(db_, "INSERT OR IGNORE INTO shape_type_stats(type,count) VALUES(0,0),(1,0),(2,0)");
    sql(db_, R"SQL(CREATE TRIGGER IF NOT EXISTS shapes_insert AFTER INSERT ON shapes BEGIN
        UPDATE shape_type_stats SET count=count+1 WHERE type=NEW.type; END)SQL");
    sql(db_, R"SQL(CREATE TRIGGER IF NOT EXISTS shapes_delete AFTER DELETE ON shapes BEGIN
        UPDATE shape_type_stats SET count=count-1 WHERE type=OLD.type; END)SQL");
    sql(db_, R"SQL(CREATE TRIGGER IF NOT EXISTS shapes_type_update AFTER UPDATE OF type ON shapes
        WHEN OLD.type <> NEW.type BEGIN
        UPDATE shape_type_stats SET count=count-1 WHERE type=OLD.type;
        UPDATE shape_type_stats SET count=count+1 WHERE type=NEW.type; END)SQL");
    // A read-only SQL view keeps QSqlTableModel compatible with joined and aggregate data.
    sql(db_, R"SQL(CREATE VIEW IF NOT EXISTS shape_overview AS
        SELECT s.id, s.type,
            COALESCE((SELECT group_concat(other_id, ', ') FROM (
                SELECT second_id AS other_id FROM connections WHERE first_id=s.id
                UNION ALL SELECT first_id AS other_id FROM connections WHERE second_id=s.id
                ORDER BY other_id)), '') AS linked_ids,
            t.count AS type_count, s.width, s.height, s.vertices
        FROM shapes s JOIN shape_type_stats t ON t.type=s.type)SQL");
    transaction.commit();
}

QVector<ShapeRecord> Database::shapes() const {
    QSqlQuery query(db_);
    if (!query.exec("SELECT id,type,x,y,width,height,vertices FROM shapes ORDER BY id"))
        fail(query.lastError().text());
    QVector<ShapeRecord> result;
    while (query.next()) result.push_back({query.value(0).toLongLong(),
        static_cast<ShapeType>(query.value(1).toInt()),
        {query.value(2).toDouble(), query.value(3).toDouble()},
        {query.value(4).toDouble(), query.value(5).toDouble()}, query.value(6).toInt()});
    return result;
}

QVector<ConnectionRecord> Database::connections() const {
    QSqlQuery query(db_);
    if (!query.exec("SELECT id,first_id,second_id FROM connections ORDER BY id"))
        fail(query.lastError().text());
    QVector<ConnectionRecord> result;
    while (query.next()) result.push_back({query.value(0).toLongLong(),
        query.value(1).toLongLong(), query.value(2).toLongLong()});
    return result;
}

std::array<int, 3> Database::counts() const {
    QSqlQuery query(db_);
    if (!query.exec("SELECT type,count FROM shape_type_stats")) fail(query.lastError().text());
    std::array<int, 3> result{};
    while (query.next()) result.at(query.value(0).toInt()) = query.value(1).toInt();
    return result;
}

qint64 Database::addShape(const ShapeRecord &shape) {
    if (!std::isfinite(shape.position.x()) || !std::isfinite(shape.position.y()) ||
        !std::isfinite(shape.size.width()) || !std::isfinite(shape.size.height()))
        fail(QStringLiteral("Координаты и размеры должны быть конечными числами."));
    QSqlQuery query(db_);
    query.prepare("INSERT INTO shapes(type,x,y,width,height,vertices) VALUES(?,?,?,?,?,?)");
    query.addBindValue(static_cast<int>(shape.type));
    query.addBindValue(shape.position.x());
    query.addBindValue(shape.position.y());
    query.addBindValue(shape.size.width());
    query.addBindValue(shape.size.height());
    query.addBindValue(shape.vertices);
    execute(query);
    return query.lastInsertId().toLongLong();
}

void Database::moveShape(qint64 id, QPointF position) {
    if (!std::isfinite(position.x()) || !std::isfinite(position.y()))
        fail(QStringLiteral("Недопустимые координаты."));
    QSqlQuery query(db_);
    query.prepare("UPDATE shapes SET x=?, y=? WHERE id=?");
    query.addBindValue(position.x()); query.addBindValue(position.y()); query.addBindValue(id);
    execute(query);
    if (query.numRowsAffected() != 1) fail(QStringLiteral("Фигура не найдена."));
}

void Database::moveShapes(const QMap<qint64, QPointF> &positions) {
    Transaction transaction(db_);
    for (auto it=positions.cbegin(); it!=positions.cend(); ++it) moveShape(it.key(),it.value());
    transaction.commit();
}

void Database::removeShapes(const QVector<qint64> &ids) {
    Transaction transaction(db_);
    QSqlQuery query(db_);
    query.prepare("DELETE FROM shapes WHERE id=?");
    for (qint64 id : ids) { query.bindValue(0, id); execute(query); }
    transaction.commit();
}

void Database::connectShapes(qint64 first, qint64 second) {
    if (first == second) fail(QStringLiteral("Выберите две разные фигуры."));
    if (first > second) std::swap(first, second);
    QSqlQuery exists(db_);
    exists.prepare("SELECT 1 FROM connections WHERE first_id=? AND second_id=?");
    exists.addBindValue(first); exists.addBindValue(second); execute(exists);
    if (exists.next()) fail(QStringLiteral("Эти фигуры уже соединены."));
    QSqlQuery query(db_);
    query.prepare("INSERT INTO connections(first_id,second_id) VALUES(?,?)");
    query.addBindValue(first); query.addBindValue(second); execute(query);
}

void Database::addDemo() {
    if (!shapes().isEmpty()) fail(QStringLiteral("Пример можно добавить только в пустую базу."));
    Transaction transaction(db_);
    QVector<qint64> ids;
    const QVector<ShapeRecord> records = {
        {0, ShapeType::Rectangle, {-240,-130}, {140,90},5},
        {0, ShapeType::Ellipse, {0,-130}, {130,90},5},
        {0, ShapeType::Polygon, {240,-130}, {120,110},6},
        {0, ShapeType::Rectangle, {-240,100}, {140,90},5},
        {0, ShapeType::Ellipse, {0,100}, {130,90},5},
        {0, ShapeType::Polygon, {240,100}, {120,110},5}
    };
    for (const auto &record : records) ids.push_back(addShape(record));
    connectShapes(ids[0],ids[1]); connectShapes(ids[1],ids[2]);
    connectShapes(ids[0],ids[3]); connectShapes(ids[1],ids[4]);
    connectShapes(ids[4],ids[5]);
    transaction.commit();
}
