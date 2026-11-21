#include "mainwindow.h"
#include "shapeitem.h"
#include "shapemodel.h"
#include <QComboBox>
#include <QFormLayout>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QSqlError>
#include <QStatusBar>
#include <QTableView>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <cmath>
#include <stdexcept>

namespace {
class CanvasView final : public QGraphicsView {
public:
    using QGraphicsView::QGraphicsView;
protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override {
        painter->fillRect(rect,QColor("#f7f9fc"));
        painter->setPen(QPen(QColor("#dce2ec"),1));
        constexpr int step=28;
        for (qreal x=std::floor(rect.left()/step)*step; x<rect.right(); x+=step)
            for (qreal y=std::floor(rect.top()/step)*step; y<rect.bottom(); y+=step)
                painter->drawPoint(QPointF(x,y));
    }
    void wheelEvent(QWheelEvent *event) override {
        if (event->modifiers() & Qt::ControlModifier) {
            const double factor = event->angleDelta().y()>0 ? 1.15 : 1/1.15;
            const double zoom = transform().m11()*factor;
            if (zoom>=0.2 && zoom<=3) scale(factor,factor);
            event->accept();
        } else QGraphicsView::wheelEvent(event);
    }
};
QLabel *label(const QString &text, const QString &name = {}) {
    auto *widget = new QLabel(text); widget->setObjectName(name); return widget;
}
QSpinBox *spin(const QString &name, int min, int max, int value) {
    auto *widget = new QSpinBox; widget->setObjectName(name);
    widget->setRange(min,max); widget->setValue(value); return widget;
}
QPushButton *button(const QString &text, const QString &name) {
    auto *widget=new QPushButton(text); widget->setObjectName(name);
    widget->setCursor(Qt::PointingHandCursor); return widget;
}
}

MainWindow::MainWindow(Database &database, QWidget *parent) : QMainWindow(parent), database_(database) {
    setWindowTitle(QStringLiteral("Редактор фигур · Qt / SQLite"));
    resize(1280,850); setMinimumSize(1000,720);
    buildUi();
    refresh();
}

MainWindow::~MainWindow() {
    // The central widget (including table) may be destroyed before the scene.
    // Stop selection synchronization while all widgets are still alive.
    syncing_=true;
    disconnect(scene_,nullptr,this,nullptr);
    scene_->clear();
    items_.clear();
}

void MainWindow::buildUi() {
    setStyleSheet(QStringLiteral(R"CSS(
        QMainWindow, QWidget { background: #f2f4f8; color: #25324b; font-size: 13px; }
        QWidget#sidebar { background: #17243d; }
        QWidget#sidebar QLabel { background: transparent; color: #d8e1f2; }
        QLabel#brand { font-size: 25px; font-weight: 700; color: white; }
        QLabel#eyebrow { font-size: 11px; color: #9cace0; font-weight: 700; }
        QLabel#section { font-size: 16px; font-weight: 700; }
        QLabel#muted { color: #718096; }
        QLabel#summary { color: #52627d; font-size: 13px; }
        QSpinBox, QComboBox { background: white; color: #25324b; border: 1px solid #d4dce8;
            border-radius: 5px; padding: 6px; min-height: 22px; }
        QSpinBox:focus, QComboBox:focus { border: 1px solid #7665df; }
        QPushButton { background: white; border: 1px solid #d4dce8; border-radius: 6px;
            padding: 9px 12px; font-weight: 600; }
        QPushButton:hover { background: #e9edf8; border-color: #9aace0; }
        QPushButton:pressed { background: #dce3f6; }
        QPushButton:disabled { color: #9aa5b5; background: #edf0f5; border-color: #e1e5ec; }
        QPushButton#addButton { background: #7866ee; color: white; border: none; padding: 12px; }
        QPushButton#addButton:hover { background: #8a79ff; }
        QPushButton#deleteButton:enabled { color: #ba3850; }
        QTableView { background: white; alternate-background-color: #f8f9fc; border: 1px solid #dfe5ee;
            gridline-color: #eef1f6; selection-background-color: #6556cf; selection-color: white; }
        QHeaderView::section { background: #edf1f7; color: #53627b; padding: 10px 7px;
            border: none; border-bottom: 1px solid #dfe5ee; font-weight: 600; }
        QGraphicsView { border: 1px solid #dfe5ee; border-radius: 6px; }
        QStatusBar { background: #e8edf5; color: #52627d; }
        QSplitter::handle { background: #e2e7f0; }
        QScrollBar:vertical { background: #edf1f7; width: 10px; margin: 0; }
        QScrollBar:horizontal { background: #edf1f7; height: 10px; margin: 0; }
        QScrollBar::handle { background: #b6c2d5; border-radius: 4px; min-height: 24px; min-width: 24px; }
        QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
        QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
    )CSS"));
    auto *central=new QWidget; setCentralWidget(central);
    auto *root=new QHBoxLayout(central); root->setContentsMargins(0,0,0,0); root->setSpacing(0);
    auto *sidebar=new QWidget; sidebar->setObjectName("sidebar"); sidebar->setFixedWidth(260);
    auto *side=new QVBoxLayout(sidebar); side->setContentsMargins(22,28,22,22); side->setSpacing(15);
    side->addWidget(label("QT / SQLITE", "eyebrow"));
    side->addWidget(label("Редактор\nфигур", "brand"));
    auto *intro=label("Создавайте фигуры, соединяйте\nи исследуйте данные.");
    intro->setWordWrap(true); side->addWidget(intro); side->addSpacing(12);
    side->addWidget(label("Новая фигура", "section"));
    shapeType_=new QComboBox; shapeType_->setObjectName("shapeType");
    for (int i=0;i<3;++i) shapeType_->addItem(shapeTypeName(static_cast<ShapeType>(i)),i);
    side->addWidget(shapeType_);
    auto *form=new QFormLayout; form->setSpacing(12);
    width_=spin("shapeWidth",20,500,140); height_=spin("shapeHeight",20,500,90);
    vertices_=spin("shapeVertices",3,12,5); vertices_->setEnabled(false);
    form->addRow("Ширина",width_); form->addRow("Высота",height_); form->addRow("Вершины",vertices_);
    side->addLayout(form);
    auto *add=button("+  Добавить фигуру", "addButton"); side->addWidget(add);
    side->addSpacing(18); side->addWidget(label("Выделение", "section"));
    selectionLabel_=label("Ничего не выбрано"); selectionLabel_->setWordWrap(true); side->addWidget(selectionLabel_);
    connectButton_=button("Соединить две фигуры", "connectButton"); side->addWidget(connectButton_);
    deleteButton_=button("Удалить выбранное", "deleteButton"); side->addWidget(deleteButton_);
    auto *help=label("Ctrl / ⌘ + щелчок — выбрать несколько.\nПеретаскивание — переместить.\nCtrl + колесо — масштаб.");
    help->setWordWrap(true); side->addWidget(help); side->addStretch();
    demoButton_=button("Загрузить пример", "demoButton"); side->addWidget(demoButton_);
    side->addWidget(label("Изменения сохраняются\nавтоматически в SQLite."));
    root->addWidget(sidebar);

    auto *workspace=new QWidget; auto *layout=new QVBoxLayout(workspace);
    layout->setContentsMargins(22,22,22,14); layout->setSpacing(12);
    auto *top=new QHBoxLayout;
    auto *heading=new QVBoxLayout;
    heading->addWidget(label("Рабочая сцена", "section"));
    summary_=label({},"summary"); heading->addWidget(summary_); top->addLayout(heading); top->addStretch();
    auto *fit=button("Показать всё", "fitButton"); top->addWidget(fit); layout->addLayout(top);
    auto *splitter=new QSplitter(Qt::Vertical); layout->addWidget(splitter,1);
    scene_=new QGraphicsScene(this); scene_->setObjectName("shapeScene");
    view_=new CanvasView(scene_); view_->setObjectName("shapeView");
    view_->setRenderHint(QPainter::Antialiasing); view_->setDragMode(QGraphicsView::RubberBandDrag);
    view_->setTransformationAnchor(QGraphicsView::AnchorUnderMouse); view_->setMinimumHeight(250);
    splitter->addWidget(view_);
    auto *tablePanel=new QWidget; auto *tableLayout=new QVBoxLayout(tablePanel);
    tableLayout->setContentsMargins(0,14,0,0); tableLayout->setSpacing(10);
    auto *tableTitle=new QHBoxLayout; tableTitle->addWidget(label("Фигуры в базе", "section"));
    tableTitle->addStretch(); tableSummary_=label({},"muted"); tableTitle->addWidget(tableSummary_);
    tableLayout->addLayout(tableTitle);
    auto *filters=new QHBoxLayout; filters->setSpacing(8);
    filterId_=spin("filterId",0,2147483647,0); filterId_->setSpecialValueText("Все ID"); filterId_->setMaximumWidth(110);
    filterType_=new QComboBox; filterType_->setObjectName("filterType"); filterType_->addItem("Все типы",-1);
    for (int i=0;i<3;++i) filterType_->addItem(shapeTypeName(static_cast<ShapeType>(i)),i);
    filterMin_=spin("filterMin",0,1000000,0); filterMin_->setSpecialValueText("От 0"); filterMin_->setMaximumWidth(90);
    filterMax_=spin("filterMax",0,1000000,0); filterMax_->setSpecialValueText("Без лимита"); filterMax_->setMaximumWidth(115);
    filterMin_->setToolTip("Минимальное число фигур данного типа в базе");
    filterMax_->setToolTip("Максимальное число фигур данного типа; 0 — без ограничения");
    auto *reset=button("Сбросить", "resetFilters");
    filters->addWidget(filterId_); filters->addWidget(filterType_,1); filters->addWidget(label("Количество типа:"));
    filters->addWidget(filterMin_); filters->addWidget(filterMax_); filters->addWidget(reset); tableLayout->addLayout(filters);
    model_=new ShapeTableModel(database_.connection(),this); model_->setObjectName("shapeModel");
    table_=new QTableView; table_->setObjectName("shapeTable"); table_->setModel(model_);
    table_->setItemDelegateForColumn(ShapeTableModel::Count,new ShapeCountDelegate(table_));
    table_->setSelectionBehavior(QAbstractItemView::SelectRows); table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers); table_->setAlternatingRowColors(true);
    table_->setSortingEnabled(true); table_->sortByColumn(0,Qt::AscendingOrder);
    table_->verticalHeader()->hide(); table_->verticalHeader()->setDefaultSectionSize(38);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(ShapeTableModel::Count,QHeaderView::Fixed);
    table_->setColumnWidth(ShapeTableModel::Count,155);
    table_->setColumnHidden(ShapeTableModel::Vertices,true);
    tableLayout->addWidget(table_,1);
    tableLayout->addWidget(label("Значки: 1–3 → один  ·  4–10 → два  ·  >10 → три. Фильтры меняют только таблицу.","muted"));
    splitter->addWidget(tablePanel); splitter->setSizes({420,280}); root->addWidget(workspace,1);
    statusBar()->showMessage("Готово. Добавьте фигуру или загрузите пример.");

    connect(shapeType_,&QComboBox::currentIndexChanged,this,[this](int value){vertices_->setEnabled(value==2);});
    connect(add,&QPushButton::clicked,this,[this]{guard([this]{addShape();});});
    connect(connectButton_,&QPushButton::clicked,this,[this]{guard([this]{connectSelection();});});
    connect(deleteButton_,&QPushButton::clicked,this,[this]{guard([this]{deleteSelection();});});
    connect(demoButton_,&QPushButton::clicked,this,[this]{guard([this]{database_.addDemo();refresh();fitScene();});});
    connect(fit,&QPushButton::clicked,this,&MainWindow::fitScene);
    connect(scene_,&QGraphicsScene::selectionChanged,this,&MainWindow::sceneSelectionChanged);
    connect(table_->selectionModel(),&QItemSelectionModel::selectionChanged,this,&MainWindow::tableSelectionChanged);
    for (auto *filter : {filterId_,filterMin_,filterMax_})
        connect(filter,&QSpinBox::valueChanged,this,[this]{guard([this]{applyFilters();});});
    connect(filterType_,&QComboBox::currentIndexChanged,this,[this]{guard([this]{applyFilters();});});
    connect(reset,&QPushButton::clicked,this,[this]{
        const QSignalBlocker a(filterId_), b(filterType_), c(filterMin_), d(filterMax_);
        filterId_->setValue(0);filterType_->setCurrentIndex(0);filterMin_->setValue(0);filterMax_->setValue(0);
        guard([this]{applyFilters();});
    });
}

void MainWindow::guard(const std::function<void()> &action) {
    try { action(); }
    catch (const std::exception &error) { statusBar()->showMessage(QString("Ошибка: %1").arg(QString::fromUtf8(error.what()))); }
}

void MainWindow::refresh(const QVector<qint64> &selection) {
    const auto shapes=database_.shapes(); const auto connections=database_.connections();
    syncing_=true;
    scene_->clear(); items_.clear();
    for (const auto &record : shapes) {
        auto *item=ShapeItem::create(record); scene_->addItem(item); items_.insert(record.id,item);
        item->setSelected(selection.contains(record.id));
        connect(item,&ShapeItem::positionCommitted,this,[this](qint64 id,QPointF){
            QVector<ShapeRecord> saved;
            try {
                saved=database_.shapes();
                QMap<qint64,QPointF> changed;
                // Qt moves all selected items together, but only the mouse grabber receives release.
                for (const auto &record : saved)
                    if (items_.contains(record.id) && items_[record.id]->pos()!=record.position)
                        changed.insert(record.id,items_[record.id]->pos());
                database_.moveShapes(changed);
                scene_->setSceneRect(scene_->itemsBoundingRect().adjusted(-220,-160,220,160));
                statusBar()->showMessage(QString("Положение фигуры #%1 сохранено").arg(id),3000);
            } catch (const std::exception &error) {
                // Roll back the view without deleting the object during its mouse event.
                for (const auto &record : saved)
                    if (items_.contains(record.id)) items_[record.id]->setPos(record.position);
                statusBar()->showMessage(QString("Не удалось сохранить: %1").arg(QString::fromUtf8(error.what())));
            }
        });
    }
    for (const auto &link : connections)
        if (items_.contains(link.first) && items_.contains(link.second))
            scene_->addItem(new ConnectionItem(items_[link.first],items_[link.second]));
    if (shapes.isEmpty()) {
        auto *hint=scene_->addText("Здесь появятся ваши фигуры\n\nДобавьте первую фигуру слева\nили нажмите «Загрузить пример».");
        hint->setDefaultTextColor(QColor("#8b99b0")); hint->setPos(-140,-60);
    }
    scene_->setSceneRect(scene_->itemsBoundingRect().adjusted(-220,-160,220,160));
    const auto counts=database_.counts();
    summary_->setText(QString("%1 фигур  /  %2 связей     ·     Прямоугольники %3   Эллипсы %4   Многоугольники %5")
        .arg(shapes.size()).arg(connections.size()).arg(counts[0]).arg(counts[1]).arg(counts[2]));
    demoButton_->setEnabled(shapes.isEmpty());
    syncing_=false;
    applyFilters();
}

void MainWindow::applyFilters() {
    syncing_=true;
    const bool ok=model_->applyFilter(filterId_->value(),filterType_->currentData().toInt(),filterMin_->value(),filterMax_->value());
    while (model_->canFetchMore()) model_->fetchMore();
    syncing_=false;
    if (!ok) throw std::runtime_error(model_->lastError().text().toUtf8().constData());
    tableSummary_->setText(QString("Показано: %1 / %2").arg(model_->rowCount()).arg(items_.size()));
    sceneSelectionChanged();
    if (filterMax_->value()>0 && filterMin_->value()>filterMax_->value())
        statusBar()->showMessage("Минимум больше максимума: условиям не соответствует ни одна фигура.");
    else statusBar()->showMessage("Данные сохранены. Фильтры не удаляют записи.",4000);
}

void MainWindow::addShape() {
    const int n=items_.size();
    const QPointF position=view_->mapToScene(view_->viewport()->rect().center()) + QPointF((n%4-1.5)*45,((n/4)%4-1.5)*35);
    ShapeRecord record{0,static_cast<ShapeType>(shapeType_->currentData().toInt()),position,
                       QSizeF(width_->value(),height_->value()),vertices_->value()};
    const qint64 id=database_.addShape(record); refresh({id});
    view_->ensureVisible(items_[id],60,60);
    statusBar()->showMessage(QString("Фигура #%1 добавлена и сохранена").arg(id),4000);
}
QVector<qint64> MainWindow::selectedIds() const {
    QVector<qint64> ids;
    for (auto *item : scene_->selectedItems()) if (auto *shape=dynamic_cast<ShapeItem *>(item)) ids.push_back(shape->id());
    return ids;
}
void MainWindow::deleteSelection() {
    const auto ids=selectedIds(); if (ids.isEmpty()) return;
    database_.removeShapes(ids); refresh();
    statusBar()->showMessage(QString("Удалено фигур: %1. Их связи также удалены.").arg(ids.size()),5000);
}
void MainWindow::connectSelection() {
    const auto ids=selectedIds(); if (ids.size()!=2) return;
    database_.connectShapes(ids[0],ids[1]); refresh(ids);
    statusBar()->showMessage("Связь создана и сохранена",4000);
}
void MainWindow::sceneSelectionChanged() {
    if (syncing_) return;
    syncing_=true;
    const auto ids=selectedIds();
    table_->clearSelection();
    for (int row=0;row<model_->rowCount();++row)
        if (ids.contains(model_->index(row,0).data().toLongLong()))
            table_->selectionModel()->select(model_->index(row,0),QItemSelectionModel::Select|QItemSelectionModel::Rows);
    syncing_=false; updateActions();
}
void MainWindow::tableSelectionChanged() {
    if (syncing_) return;
    syncing_=true; scene_->clearSelection();
    for (const auto &index : table_->selectionModel()->selectedRows()) {
        const qint64 id=index.data().toLongLong();
        if (items_.contains(id)) items_[id]->setSelected(true);
    }
    syncing_=false; updateActions();
}
void MainWindow::updateActions() {
    const auto ids=selectedIds();
    connectButton_->setEnabled(ids.size()==2); deleteButton_->setEnabled(!ids.isEmpty());
    QStringList names; for (qint64 id : ids) names << QString("#%1").arg(id);
    selectionLabel_->setText(ids.isEmpty() ? "Ничего не выбрано" : QString("Выбрано: %1\n%2").arg(ids.size()).arg(names.join(", ")));
}
void MainWindow::fitScene() {
    view_->fitInView(scene_->itemsBoundingRect().adjusted(-65,-65,65,65),Qt::KeepAspectRatio);
    if (view_->transform().m11()>1.3) view_->resetTransform();
}
