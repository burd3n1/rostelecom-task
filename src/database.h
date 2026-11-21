#pragma once
#include <QPointF>
#include <QMap>
#include <QSizeF>
#include <QSqlDatabase>
#include <QVector>
#include <array>

enum class ShapeType { Rectangle = 0, Ellipse = 1, Polygon = 2 };
QString shapeTypeName(ShapeType type);

struct ShapeRecord {
    qint64 id = 0;
    ShapeType type = ShapeType::Rectangle;
    QPointF position;
    QSizeF size = {120, 80};
    int vertices = 5;
};
struct ConnectionRecord { qint64 id; qint64 first; qint64 second; };

// Owns a named SQLite connection. Views/models must be destroyed before this object.
class Database {
public:
    explicit Database(const QString &path);
    ~Database();
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;
    QSqlDatabase connection() const { return db_; }
    QVector<ShapeRecord> shapes() const;
    QVector<ConnectionRecord> connections() const;
    std::array<int, 3> counts() const;
    qint64 addShape(const ShapeRecord &shape);
    void moveShape(qint64 id, QPointF position);
    void moveShapes(const QMap<qint64, QPointF> &positions);
    void removeShapes(const QVector<qint64> &ids);
    void connectShapes(qint64 first, qint64 second);
    void addDemo();
private:
    void initialize();
    QSqlDatabase db_;
    QString connectionName_;
};
