#include "database.h"
#include "mainwindow.h"
#include "shapeitem.h"
#include "shapemodel.h"
#include <QComboBox>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QImage>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QSqlQuery>
#include <QTableView>
#include <QTemporaryDir>
#include <QtTest>
#include <stdexcept>

class EditorTests final : public QObject {
    Q_OBJECT
private slots:
    void persistenceAndCascade();
    void filtersDoNotDelete();
    void invalidOperations();
    void delegateBoundaries();
    void graphicsFollowMovement();
    void groupMovementAndRollback();
    void userWorkflow();
};

void EditorTests::persistenceAndCascade() {
    QTemporaryDir dir;
    const QString path=dir.filePath("shapes.sqlite");
    qint64 a,b,c;
    {
        Database db(path);
        a=db.addShape({0,ShapeType::Rectangle,{10,20},{100,70},5});
        b=db.addShape({0,ShapeType::Ellipse,{60,40},{120,90},5});
        c=db.addShape({0,ShapeType::Polygon,{90,60},{80,110},6});
        db.connectShapes(a,b); db.connectShapes(b,c); db.connectShapes(a,c);
        db.moveShape(c,{140,160});
        QCOMPARE(db.counts()[0],1); QCOMPARE(db.counts()[1],1); QCOMPARE(db.counts()[2],1);
    }
    {
        Database db(path);
        QCOMPARE(db.shapes().size(),3); QCOMPARE(db.connections().size(),3);
        QCOMPARE(db.shapes()[2].position,QPointF(140,160));
        QCOMPARE(db.shapes()[2].vertices,6);
        db.removeShapes({b});
        QCOMPARE(db.shapes().size(),2); QCOMPARE(db.connections().size(),1);
        QCOMPARE(db.connections()[0].first,a); QCOMPARE(db.connections()[0].second,c);
        QCOMPARE(db.counts()[1],0);
        QSqlQuery query(db.connection());
        QVERIFY(query.exec("PRAGMA foreign_key_check")); QVERIFY(!query.next());
        QVERIFY(query.exec("PRAGMA integrity_check")); QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(),QString("ok"));
    }
    {
        Database db(path);
        QCOMPARE(db.shapes().size(),2); QCOMPARE(db.connections().size(),1);
        db.removeShapes({a,c}); QCOMPARE(db.shapes().size(),0); QCOMPARE(db.connections().size(),0);
        QCOMPARE(db.counts()[0],0); QCOMPARE(db.counts()[2],0);
    }
}

void EditorTests::filtersDoNotDelete() {
    Database db(":memory:");
    for (int i=0;i<11;++i) db.addShape({0,ShapeType::Rectangle,{},{120,80},5});
    for (int i=0;i<4;++i) db.addShape({0,ShapeType::Ellipse,{},{120,80},5});
    for (int i=0;i<3;++i) db.addShape({0,ShapeType::Polygon,{},{120,80},5});
    db.connectShapes(1,12); db.connectShapes(1,16);
    ShapeTableModel model(db.connection());
    QVERIFY(model.applyFilter(0,-1,0,0)); QCOMPARE(model.rowCount(),18);
    QVERIFY(model.applyFilter(0,1,0,0)); QCOMPARE(model.rowCount(),4);
    QVERIFY(model.applyFilter(0,-1,4,10)); QCOMPARE(model.rowCount(),4);
    QVERIFY(model.applyFilter(0,-1,11,0)); QCOMPARE(model.rowCount(),11);
    QVERIFY(model.applyFilter(1,-1,0,0)); QCOMPARE(model.rowCount(),1);
    QCOMPARE(model.index(0,ShapeTableModel::Count).data(Qt::EditRole).toInt(),11);
    QCOMPARE(model.index(0,ShapeTableModel::Links).data().toString(),QString("12, 16"));
    QVERIFY(model.applyFilter(1,1,0,0)); QCOMPARE(model.rowCount(),0);
    QVERIFY(model.applyFilter(0,-1,10,4)); QCOMPARE(model.rowCount(),0);
    QVERIFY(model.applyFilter(0,-1,0,0)); QCOMPARE(model.rowCount(),18);
    QCOMPARE(db.shapes().size(),18); QCOMPARE(db.connections().size(),2);
    QVERIFY(!(model.flags(model.index(0,0)) & Qt::ItemIsEditable));
}

void EditorTests::invalidOperations() {
    Database db(":memory:");
    const auto a=db.addShape({}); const auto b=db.addShape({});
    db.connectShapes(b,a);
    QVERIFY_EXCEPTION_THROWN(db.connectShapes(a,b),std::runtime_error);
    QVERIFY_EXCEPTION_THROWN(db.connectShapes(a,a),std::runtime_error);
    QVERIFY_EXCEPTION_THROWN(db.connectShapes(a,999),std::runtime_error);
    QVERIFY_EXCEPTION_THROWN(db.addShape({0,ShapeType::Ellipse,{},{-1,20},5}),std::runtime_error);
    QVERIFY_EXCEPTION_THROWN(db.addShape({0,ShapeType::Polygon,{},{30,30},2}),std::runtime_error);
    QCOMPARE(db.shapes().size(),2); QCOMPARE(db.counts()[0],2); QCOMPARE(db.connections().size(),1);
    QVERIFY_EXCEPTION_THROWN(db.addDemo(),std::runtime_error);
    QCOMPARE(db.shapes().size(),2);
}

void EditorTests::delegateBoundaries() {
    QCOMPARE(ShapeCountDelegate::iconCount(0),0);
    QCOMPARE(ShapeCountDelegate::iconCount(1),1);
    QCOMPARE(ShapeCountDelegate::iconCount(3),1);
    QCOMPARE(ShapeCountDelegate::iconCount(4),2);
    QCOMPARE(ShapeCountDelegate::iconCount(10),2);
    QCOMPARE(ShapeCountDelegate::iconCount(11),3);
    Database db(":memory:");
    for (int i=0;i<11;++i) db.addShape({0,ShapeType::Polygon,{},{100,80},5});
    ShapeTableModel model(db.connection()); QVERIFY(model.select());
    QImage image(180,40,QImage::Format_ARGB32); image.fill(Qt::white);
    QPainter painter(&image);
    QStyleOptionViewItem option; option.rect=QRect(0,0,180,40); option.state=QStyle::State_Enabled;
    ShapeCountDelegate delegate; delegate.paint(&painter,option,model.index(0,ShapeTableModel::Count));
    painter.end();
    // Each of the three distinct icon regions must actually contain rendered ink.
    for (int x : {12,37,62}) {
        bool ink=false;
        for (int px=x;px<x+18;++px) for (int py=11;py<29;++py)
            if (image.pixelColor(px,py)!=Qt::white) ink=true;
        QVERIFY(ink);
    }
}

void EditorTests::graphicsFollowMovement() {
    QGraphicsScene scene;
    auto *a=ShapeItem::create({1,ShapeType::Rectangle,{10,20},{100,80},5});
    auto *b=ShapeItem::create({2,ShapeType::Polygon,{200,20},{100,80},6});
    scene.addItem(a); scene.addItem(b);
    auto *edge=new ConnectionItem(a,b); scene.addItem(edge);
    QCOMPARE(edge->line(),QLineF(QPointF(10,20),QPointF(200,20)));
    a->setPos(50,70); QCOMPARE(edge->line().p1(),QPointF(50,70));
    b->setPos(100,140); QCOMPARE(edge->line().p2(),QPointF(100,140));
    QVERIFY(dynamic_cast<RectangleItem *>(a)); QVERIFY(dynamic_cast<PolygonItem *>(b));
    QVERIFY(!b->outline().isEmpty());
}

void EditorTests::userWorkflow() {
    Database db(":memory:");
    MainWindow window(db); window.show(); QTest::qWait(100);
    auto *add=window.findChild<QPushButton *>("addButton");
    auto *type=window.findChild<QComboBox *>("shapeType");
    auto *scene=window.findChild<QGraphicsScene *>("shapeScene");
    auto *table=window.findChild<QTableView *>("shapeTable");
    auto *connectButton=window.findChild<QPushButton *>("connectButton");
    auto *remove=window.findChild<QPushButton *>("deleteButton");
    QVERIFY(add && type && scene && table && connectButton && remove);
    QVERIFY(!remove->isEnabled());
    for (int i=0;i<3;++i) {type->setCurrentIndex(i);QTest::mouseClick(add,Qt::LeftButton);}
    QCOMPARE(db.shapes().size(),3); QCOMPARE(table->model()->rowCount(),3);
    scene->clearSelection();
    for (auto *item:scene->items()) if (auto *shape=dynamic_cast<ShapeItem *>(item))
        shape->setSelected(shape->id()==1 || shape->id()==2);
    QVERIFY(connectButton->isEnabled());
    QTest::mouseClick(connectButton,Qt::LeftButton); QCOMPARE(db.connections().size(),1);
    auto *filter=window.findChild<QComboBox *>("filterType"); filter->setCurrentIndex(2);
    QCOMPARE(table->model()->rowCount(),1); QCOMPARE(db.shapes().size(),3);
    auto *reset=window.findChild<QPushButton *>("resetFilters"); QTest::mouseClick(reset,Qt::LeftButton);
    QCOMPARE(table->model()->rowCount(),3);
    table->clearSelection(); table->selectRow(0);
    QCOMPARE(scene->selectedItems().size(),1); QVERIFY(remove->isEnabled());
    QTest::mouseClick(remove,Qt::LeftButton);
    QCOMPARE(db.shapes().size(),2); QCOMPARE(db.connections().size(),0); QCOMPARE(table->model()->rowCount(),2);
    int shapeCount=0,edgeCount=0;
    for (auto *item:scene->items()) {
        if (dynamic_cast<ShapeItem *>(item)) ++shapeCount;
        if (dynamic_cast<ConnectionItem *>(item)) ++edgeCount;
    }
    QCOMPARE(shapeCount,2); QCOMPARE(edgeCount,0);
}

void EditorTests::groupMovementAndRollback() {
    Database db(":memory:");
    db.addShape({0,ShapeType::Rectangle,{-180,0},{100,80},5});
    db.addShape({0,ShapeType::Ellipse,{180,0},{100,80},5});
    db.connectShapes(1,2);
    MainWindow window(db); window.show(); QTest::qWait(50);
    auto *scene=window.findChild<QGraphicsScene *>("shapeScene");
    auto *view=window.findChild<QGraphicsView *>("shapeView");
    ShapeItem *first=nullptr;
    for (auto *item:scene->items()) if (auto *shape=dynamic_cast<ShapeItem *>(item)) {
        shape->setSelected(true);
        if (shape->id()==1) first=shape;
    }
    QVERIFY(first);
    const QPoint begin=view->mapFromScene(first->pos());
    QTest::mousePress(view->viewport(),Qt::LeftButton,Qt::NoModifier,begin);
    QTest::mouseMove(view->viewport(),begin+QPoint(60,45),30);
    QTest::mouseRelease(view->viewport(),Qt::LeftButton,Qt::NoModifier,begin+QPoint(60,45));
    const auto moved=db.shapes();
    QVERIFY(moved[0].position!=QPointF(-180,0));
    QCOMPARE(moved[1].position-QPointF(180,0),moved[0].position-QPointF(-180,0));
    const QMap<qint64,QPointF> invalid{{1,{10,20}},{999,{10,20}}};
    QVERIFY_EXCEPTION_THROWN(db.moveShapes(invalid),std::runtime_error);
    QCOMPARE(db.shapes()[0].position,moved[0].position);
}

QTEST_MAIN(EditorTests)
#include "editor_tests.moc"
