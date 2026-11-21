#include "shapeitem.h"
#include <QCursor>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <cmath>

QColor shapeColor(ShapeType type) {
    switch (type) {
    case ShapeType::Rectangle: return QColor("#6457d9");
    case ShapeType::Ellipse: return QColor("#087f8c");
    case ShapeType::Polygon: return QColor("#c77d19");
    }
    return Qt::gray;
}

QPainterPath shapePath(ShapeType type, const QRectF &rect, int vertices) {
    QPainterPath path;
    if (type == ShapeType::Rectangle) path.addRect(rect);
    else if (type == ShapeType::Ellipse) path.addEllipse(rect);
    else {
        constexpr double pi = 3.14159265358979323846;
        for (int i = 0; i < vertices; ++i) {
            const double angle = -pi / 2 + 2 * pi * i / vertices;
            const QPointF point = rect.center() + QPointF(std::cos(angle)*rect.width()/2,
                                                         std::sin(angle)*rect.height()/2);
            if (i == 0) path.moveTo(point); else path.lineTo(point);
        }
        path.closeSubpath();
    }
    return path;
}

ShapeItem::ShapeItem(const ShapeRecord &record) : record_(record) {
    setFlags(ItemIsSelectable | ItemIsMovable | ItemSendsGeometryChanges);
    setPos(record.position);
    setZValue(1);
    setCursor(Qt::OpenHandCursor);
    setToolTip(QString("%1 #%2\n%3 × %4\nПеретащите для перемещения")
        .arg(shapeTypeName(record.type)).arg(record.id)
        .arg(record.size.width()).arg(record.size.height()));
}
QRectF ShapeItem::bodyRect() const {
    return {-record_.size.width()/2, -record_.size.height()/2,
            record_.size.width(), record_.size.height()};
}
QRectF ShapeItem::boundingRect() const { return bodyRect().adjusted(-5,-5,5,5); }
QPainterPath ShapeItem::shape() const { return outline(); }
void ShapeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    painter->setRenderHint(QPainter::Antialiasing);
    const QColor color = shapeColor(record_.type);
    QColor fill = color; fill.setAlpha(isSelected() ? 70 : 30);
    painter->setBrush(fill);
    painter->setPen(QPen(color, isSelected() ? 3 : 1.8));
    painter->drawPath(outline());
    if (isSelected()) {
        painter->setPen(QPen(color, 1, Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(bodyRect().adjusted(-4,-4,4,4));
    }
    painter->setPen(color.darker(150));
    QFont font = painter->font(); font.setBold(true); font.setPointSize(10);
    painter->setFont(font);
    painter->drawText(bodyRect(), Qt::AlignCenter, QString("#%1").arg(record_.id));
}
QVariant ShapeItem::itemChange(GraphicsItemChange change, const QVariant &value) {
    if (change == ItemPositionHasChanged) emit positionChanged();
    return QGraphicsObject::itemChange(change, value);
}
void ShapeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    QGraphicsObject::mouseReleaseEvent(event);
    if (record_.position != pos()) {
        emit positionCommitted(id(), pos());
        record_.position = pos();
    }
}
ShapeItem *ShapeItem::create(const ShapeRecord &record) {
    switch (record.type) {
    case ShapeType::Rectangle: return new RectangleItem(record);
    case ShapeType::Ellipse: return new EllipseItem(record);
    case ShapeType::Polygon: return new PolygonItem(record);
    }
    return nullptr;
}
QPainterPath RectangleItem::outline() const { return shapePath(ShapeType::Rectangle,bodyRect()); }
QPainterPath EllipseItem::outline() const { return shapePath(ShapeType::Ellipse,bodyRect()); }
QPainterPath PolygonItem::outline() const { return shapePath(ShapeType::Polygon,bodyRect(),record_.vertices); }

ConnectionItem::ConnectionItem(ShapeItem *first, ShapeItem *second) : first_(first), second_(second) {
    setZValue(-1);
    setAcceptedMouseButtons(Qt::NoButton);
    connect(first, &ShapeItem::positionChanged, this, &ConnectionItem::updateLine);
    connect(second, &ShapeItem::positionChanged, this, &ConnectionItem::updateLine);
    updateLine();
}
void ConnectionItem::updateLine() {
    prepareGeometryChange();
    line_ = QLineF(first_->scenePos(), second_->scenePos());
    update();
}
QRectF ConnectionItem::boundingRect() const {
    return QRectF(line_.p1(), line_.p2()).normalized().adjusted(-4,-4,4,4);
}
void ConnectionItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(QColor("#94a3b8"), 2, Qt::SolidLine, Qt::RoundCap));
    painter->drawLine(line_);
}
