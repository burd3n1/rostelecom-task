#pragma once
#include "database.h"
#include <QGraphicsObject>
#include <QGraphicsLineItem>
#include <QPainterPath>

QColor shapeColor(ShapeType type);
QPainterPath shapePath(ShapeType type, const QRectF &rect, int vertices = 5);

class ShapeItem : public QGraphicsObject {
    Q_OBJECT
public:
    explicit ShapeItem(const ShapeRecord &record);
    qint64 id() const { return record_.id; }
    ShapeType shapeType() const { return record_.type; }
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) override;
    virtual QPainterPath outline() const = 0;
    static ShapeItem *create(const ShapeRecord &record);
signals:
    void positionChanged();
    void positionCommitted(qint64 id, QPointF position);
protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    QRectF bodyRect() const;
    ShapeRecord record_;
};
class RectangleItem final : public ShapeItem {
public:
    using ShapeItem::ShapeItem;
    QPainterPath outline() const override;
};
class EllipseItem final : public ShapeItem {
public:
    using ShapeItem::ShapeItem;
    QPainterPath outline() const override;
};
class PolygonItem final : public ShapeItem {
public:
    using ShapeItem::ShapeItem;
    QPainterPath outline() const override;
};
class ConnectionItem final : public QGraphicsObject {
    Q_OBJECT
public:
    ConnectionItem(ShapeItem *first, ShapeItem *second);
    QRectF boundingRect() const override;
    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) override;
    QLineF line() const { return line_; }
private:
    void updateLine();
    ShapeItem *first_;
    ShapeItem *second_;
    QLineF line_;
};
