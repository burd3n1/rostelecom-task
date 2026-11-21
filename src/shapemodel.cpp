#include "shapemodel.h"
#include "shapeitem.h"
#include <QApplication>
#include <QPainter>

ShapeTableModel::ShapeTableModel(QSqlDatabase db, QObject *parent) : QSqlTableModel(parent, db) {
    setTable("shape_overview");
    const QStringList headers = {"ID", "Тип фигуры", "Связи с ID", "Количество типа", "Ширина", "Высота", "Вершины"};
    for (int i=0; i<headers.size(); ++i) setHeaderData(i,Qt::Horizontal,headers[i]);
    setSort(Id,Qt::AscendingOrder);
}
QVariant ShapeTableModel::data(const QModelIndex &index, int role) const {
    if (role == Qt::DisplayRole && index.column() == Type)
        return shapeTypeName(static_cast<ShapeType>(QSqlTableModel::data(index,Qt::EditRole).toInt()));
    if (role == Qt::DisplayRole && index.column() == Links) {
        const auto value = QSqlTableModel::data(index,role).toString();
        return value.isEmpty() ? QStringLiteral("—") : value;
    }
    if (role == Qt::ToolTipRole && index.column() == Count)
        return QString("Всего фигур этого типа: %1\n1–3: один значок; 4–10: два; >10: три.")
            .arg(QSqlTableModel::data(index,Qt::EditRole).toInt());
    if (role == Qt::DisplayRole && index.column() == Vertices &&
        QSqlTableModel::data(this->index(index.row(),Type),Qt::EditRole).toInt() != 2)
        return QStringLiteral("—");
    if (role == Qt::TextAlignmentRole && index.column() != Type && index.column() != Links)
        return int(Qt::AlignCenter);
    return QSqlTableModel::data(index,role);
}
Qt::ItemFlags ShapeTableModel::flags(const QModelIndex &index) const {
    return QSqlTableModel::flags(index) & ~Qt::ItemIsEditable;
}
bool ShapeTableModel::applyFilter(qint64 id, int type, int minCount, int maxCount) {
    QStringList conditions;
    // Values come from numeric widgets; no user text is interpolated into SQL.
    if (id > 0) conditions << QString("id=%1").arg(id);
    if (type >= 0) conditions << QString("type=%1").arg(type);
    if (minCount > 0) conditions << QString("type_count>=%1").arg(minCount);
    if (maxCount > 0) conditions << QString("type_count<=%1").arg(maxCount);
    setFilter(conditions.join(" AND "));
    return select();
}
int ShapeCountDelegate::iconCount(int count) {
    return count <= 0 ? 0 : count <= 3 ? 1 : count <= 10 ? 2 : 3;
}
void ShapeCountDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                               const QModelIndex &index) const {
    QStyleOptionViewItem styled(option);
    initStyleOption(&styled,index);
    styled.text.clear();
    const QStyle *style = option.widget ? option.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem,&styled,painter,option.widget);
    const int count = index.data(Qt::EditRole).toInt();
    const auto type = static_cast<ShapeType>(index.siblingAtColumn(ShapeTableModel::Type).data(Qt::EditRole).toInt());
    painter->save();
    painter->setClipRect(option.rect);
    painter->setRenderHint(QPainter::Antialiasing);
    QColor color = shapeColor(type);
    if (option.state & QStyle::State_Selected) color = option.palette.highlightedText().color();
    QColor fill = color; fill.setAlpha(40);
    painter->setPen(QPen(color,1.5)); painter->setBrush(fill);
    for (int i=0; i<iconCount(count); ++i) {
        const QRectF rect(option.rect.left()+12+i*25,option.rect.center().y()-8,17,17);
        painter->drawPath(shapePath(type,rect));
    }
    painter->setPen(option.state & QStyle::State_Selected ? option.palette.highlightedText().color() : QColor("#475569"));
    painter->drawText(option.rect.adjusted(92,0,-12,0),Qt::AlignRight|Qt::AlignVCenter,QString::number(count));
    painter->restore();
}
QSize ShapeCountDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const { return {150,38}; }
