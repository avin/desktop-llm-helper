#include "modelselectbox.h"

#include <QAbstractItemView>
#include <QAbstractListModel>
#include <QApplication>
#include <QColor>
#include <QEvent>
#include <QFrame>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QScrollBar>
#include <QScreen>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QStyle>
#include <QStyleOptionButton>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace {
constexpr int kPopupHeight = 300;
constexpr int kRowHeight = 30;
constexpr int kTooltipWidth = 320;

QString displayNameFor(const ModelInfo &model) {
    return model.id.isEmpty() ? model.name : model.id;
}

QString valueOrFallback(const QString &value) {
    const QString trimmed = value.trimmed();
    return trimmed.isEmpty() ? QStringLiteral("N/A") : trimmed;
}

QString pricingValueFor(const ModelInfo &model) {
    QStringList parts;
    if (!model.promptPrice.trimmed().isEmpty())
        parts.append(QStringLiteral("Prompt: %1").arg(model.promptPrice.trimmed()));
    if (!model.completionPrice.trimmed().isEmpty())
        parts.append(QStringLiteral("Completion: %1").arg(model.completionPrice.trimmed()));
    if (!model.inputCacheReadPrice.trimmed().isEmpty())
        parts.append(QStringLiteral("Cache read: %1").arg(model.inputCacheReadPrice.trimmed()));
    return parts.isEmpty() ? QStringLiteral("N/A") : parts.join(QStringLiteral("; "));
}

QString richFieldText(const QString &label, const QString &value) {
    return QStringLiteral("<b>%1:</b> %2")
        .arg(label.toHtmlEscaped(), valueOrFallback(value).toHtmlEscaped());
}

bool modelLessThan(const ModelInfo &left, const ModelInfo &right) {
    return QString::localeAwareCompare(displayNameFor(left).toLower(), displayNameFor(right).toLower()) < 0;
}
}

enum ModelListRole {
    ModelIdRole = Qt::UserRole + 1,
    NameRole,
    DescriptionRole,
    PromptPriceRole,
    CompletionPriceRole,
    InputCacheReadPriceRole,
    KnowledgeCutoffRole,
    IsEmptyRole
};

class ModelListModel : public QAbstractListModel {
public:
    explicit ModelListModel(QObject *parent = nullptr)
        : QAbstractListModel(parent) {
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : models.size() + 1;
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
            return QVariant();

        if (index.row() == 0) {
            if (role == Qt::DisplayRole)
                return emptyLabelText;
            if (role == ModelIdRole)
                return QString();
            if (role == IsEmptyRole)
                return true;
            return QVariant();
        }

        const ModelInfo &model = models.at(index.row() - 1);
        switch (role) {
        case Qt::DisplayRole:
            return displayNameFor(model);
        case ModelIdRole:
            return model.id;
        case NameRole:
            return model.name;
        case DescriptionRole:
            return model.description;
        case PromptPriceRole:
            return model.promptPrice;
        case CompletionPriceRole:
            return model.completionPrice;
        case InputCacheReadPriceRole:
            return model.inputCacheReadPrice;
        case KnowledgeCutoffRole:
            return model.knowledgeCutoff;
        case IsEmptyRole:
            return false;
        default:
            return QVariant();
        }
    }

    void setModels(const ModelInfoList &newModels) {
        beginResetModel();
        models = newModels;
        endResetModel();
    }

    void setEmptyLabel(const QString &label) {
        if (emptyLabelText == label)
            return;
        emptyLabelText = label;
        const QModelIndex emptyIndex = index(0, 0);
        emit dataChanged(emptyIndex, emptyIndex);
    }

    ModelInfo modelAtRow(int row) const {
        if (row <= 0 || row > models.size())
            return ModelInfo();
        return models.at(row - 1);
    }

private:
    ModelInfoList models;
    QString emptyLabelText = QStringLiteral("Not selected");
};

class ModelListProxyModel : public QSortFilterProxyModel {
public:
    explicit ModelListProxyModel(QObject *parent = nullptr)
        : QSortFilterProxyModel(parent) {
        setDynamicSortFilter(false);
    }

    void setFilterText(const QString &text) {
        const QString normalized = text.trimmed().toLower();
        if (filterText == normalized)
            return;
        filterText = normalized;
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override {
        if (sourceRow == 0 || filterText.isEmpty())
            return true;

        const QModelIndex sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
        const QString displayName = sourceIndex.data(Qt::DisplayRole).toString().toLower();
        const QString modelName = sourceIndex.data(NameRole).toString().toLower();
        return displayName.contains(filterText) || modelName.contains(filterText);
    }

private:
    QString filterText;
};

class ModelListDelegate : public QStyledItemDelegate {
public:
    explicit ModelListDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent) {
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override {
        painter->save();

        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
        const bool empty = index.data(IsEmptyRole).toBool();

        if (selected)
            painter->fillRect(option.rect, option.palette.highlight());
        else if (hovered)
            painter->fillRect(option.rect, option.palette.alternateBase());

        QColor textColor = selected
            ? option.palette.highlightedText().color()
            : option.palette.text().color();
        if (empty && !selected)
            textColor = QColor(170, 170, 170);

        const QRect textRect = option.rect.adjusted(8, 0, empty ? -8 : -34, 0);
        const QString text = option.fontMetrics.elidedText(index.data(Qt::DisplayRole).toString(),
                                                           Qt::ElideRight, textRect.width());
        painter->setPen(textColor);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);

        if (!empty) {
            const QRect helpRect = infoRect(option.rect);
            const QColor borderColor = selected
                ? option.palette.highlightedText().color()
                : option.palette.mid().color();
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setPen(borderColor);
            painter->setBrush(Qt::NoBrush);
            painter->drawEllipse(helpRect);

            QFont helpFont = option.font;
            helpFont.setBold(true);
            painter->setFont(helpFont);
            painter->drawText(helpRect, Qt::AlignCenter, QStringLiteral("?"));
        }

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        Q_UNUSED(option);
        Q_UNUSED(index);
        return QSize(0, kRowHeight);
    }

    static QRect infoRect(const QRect &rowRect) {
        return QRect(rowRect.right() - 25, rowRect.y() + (rowRect.height() - 18) / 2, 18, 18);
    }
};

ModelSelectBox::ModelSelectBox(QWidget *parent)
    : QPushButton(parent)
      , noSelectionLabel(QStringLiteral("Not selected")) {
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(24);
    updateButtonLabel();
    connect(this, &QPushButton::clicked, this, &ModelSelectBox::showPopup);
}

ModelSelectBox::~ModelSelectBox() {
    delete tooltip;
}

QString ModelSelectBox::currentModel() const {
    return selectedModelId;
}

void ModelSelectBox::setCurrentModel(const QString &modelId) {
    const QString normalized = modelId.trimmed();
    if (selectedModelId == normalized)
        return;
    selectedModelId = normalized;
    updateButtonLabel();
}

void ModelSelectBox::setEmptyLabel(const QString &label) {
    const QString normalized = label.trimmed().isEmpty()
        ? QStringLiteral("Not selected")
        : label.trimmed();
    if (noSelectionLabel == normalized)
        return;
    noSelectionLabel = normalized;
    updateButtonLabel();
    if (modelListModel)
        modelListModel->setEmptyLabel(noSelectionLabel);
    if (popup && popup->isVisible())
        rebuildRows();
}

QString ModelSelectBox::emptyLabel() const {
    return noSelectionLabel;
}

int ModelSelectBox::currentReloadGeneration() const {
    return reloadGeneration;
}

void ModelSelectBox::setModels(const ModelInfoList &models) {
    loadedModels = models;
    std::sort(loadedModels.begin(), loadedModels.end(), modelLessThan);
    popupLoading = false;
    if (!popup)
        return;
    searchEdit->setEnabled(true);
    modelListModel->setModels(loadedModels);
    rebuildRows();
    stack->setCurrentWidget(modelListView);
    updateCurrentIndexLater();
}

void ModelSelectBox::setLoadError(const QString &message) {
    popupLoading = false;
    if (!popup)
        return;
    searchEdit->setEnabled(true);
    loadingLabel->setText(message.trimmed().isEmpty() ? QStringLiteral("Failed to load models.") : message);
    stack->setCurrentWidget(loadingPage);
}

bool ModelSelectBox::eventFilter(QObject *watched, QEvent *event) {
    if (watched == popup && event->type() == QEvent::Hide)
        hideTooltip();

    if (modelListView && watched == modelListView->viewport()) {
        if (event->type() == QEvent::MouseMove) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            const QModelIndex index = modelListView->indexAt(mouseEvent->pos());
            if (index.isValid() && isInfoArea(index, mouseEvent->pos())) {
                const ModelInfo model = modelForIndex(index);
                const QRect rowRect = modelListView->visualRect(index);
                const QRect helpRect = ModelListDelegate::infoRect(rowRect);
                showTooltipFor(model, QRect(modelListView->viewport()->mapToGlobal(helpRect.topLeft()),
                                            helpRect.size()));
            } else {
                hideTooltip();
            }
            return false;
        }

        if (event->type() == QEvent::Leave) {
            hideTooltip();
            return false;
        }

        if (event->type() == QEvent::MouseButtonRelease) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            const QModelIndex index = modelListView->indexAt(mouseEvent->pos());
            if (index.isValid() && isInfoArea(index, mouseEvent->pos())) {
                const ModelInfo model = modelForIndex(index);
                const QRect rowRect = modelListView->visualRect(index);
                const QRect helpRect = ModelListDelegate::infoRect(rowRect);
                showTooltipFor(model, QRect(modelListView->viewport()->mapToGlobal(helpRect.topLeft()),
                                            helpRect.size()));
                return true;
            }
        }
    }

    return QPushButton::eventFilter(watched, event);
}

void ModelSelectBox::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    QStyleOptionButton option;
    initStyleOption(&option);
    option.text.clear();

    QPainter painter(this);
    style()->drawControl(QStyle::CE_PushButton, &option, &painter, this);

    QRect textRect = rect().adjusted(8, 0, -26, 0);
    QString label = selectedModelId.isEmpty() ? noSelectionLabel : selectedModelId;
    const QString elided = fontMetrics().elidedText(label, Qt::ElideRight, textRect.width());
    style()->drawItemText(&painter, textRect, Qt::AlignLeft | Qt::AlignVCenter,
                          palette(), isEnabled(), elided, QPalette::ButtonText);

    QStyleOption arrowOption;
    arrowOption.initFrom(this);
    arrowOption.rect = QRect(width() - 20, (height() - 12) / 2, 12, 12);
    style()->drawPrimitive(QStyle::PE_IndicatorArrowDown, &arrowOption, &painter, this);
}

void ModelSelectBox::ensurePopup() {
    if (popup)
        return;

    popup = new QFrame(this, Qt::Popup | Qt::FramelessWindowHint);
    popup->setObjectName(QStringLiteral("modelSelectPopup"));
    popup->setFrameShape(QFrame::StyledPanel);
    popup->installEventFilter(this);
    popup->setStyleSheet(QStringLiteral(
        "QFrame#modelSelectPopup { background: palette(window); border: 1px solid palette(mid); }"
        "QLineEdit { margin: 6px; padding: 4px 6px; }"
    ));

    auto *layout = new QVBoxLayout(popup);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    searchEdit = new QLineEdit(popup);
    searchEdit->setPlaceholderText(QStringLiteral("Search models"));
    layout->addWidget(searchEdit);

    stack = new QStackedWidget(popup);
    layout->addWidget(stack, 1);

    loadingPage = new QWidget(stack);
    auto *loadingLayout = new QVBoxLayout(loadingPage);
    loadingLabel = new QLabel(QStringLiteral("Loading..."), loadingPage);
    loadingLabel->setAlignment(Qt::AlignCenter);
    loadingLayout->addWidget(loadingLabel, 1);
    stack->addWidget(loadingPage);

    modelListModel = new ModelListModel(this);
    modelListModel->setEmptyLabel(noSelectionLabel);
    modelListModel->setModels(loadedModels);

    proxyModel = new ModelListProxyModel(this);
    proxyModel->setSourceModel(modelListModel);

    modelListView = new QListView(stack);
    modelListView->setModel(proxyModel);
    modelListView->setItemDelegate(new ModelListDelegate(modelListView));
    modelListView->setFrameShape(QFrame::NoFrame);
    modelListView->setUniformItemSizes(true);
    modelListView->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    modelListView->setMouseTracking(true);
    modelListView->setSelectionMode(QAbstractItemView::SingleSelection);
    modelListView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    modelListView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    modelListView->viewport()->setMouseTracking(true);
    modelListView->viewport()->installEventFilter(this);
    stack->addWidget(modelListView);

    connect(searchEdit, &QLineEdit::textChanged, this, &ModelSelectBox::rebuildRows);
    connect(modelListView, &QListView::clicked, this, [this](const QModelIndex &index) {
        if (!index.isValid())
            return;
        chooseModel(index.data(ModelIdRole).toString());
    });
}

void ModelSelectBox::showPopup() {
    ensurePopup();
    hideTooltip();
    loadedModels.clear();
    modelListModel->setModels(loadedModels);
    searchEdit->clear();
    showLoading();

    const QPoint topLeft = mapToGlobal(QPoint(0, 0));
    popup->setFixedSize(width(), kPopupHeight);
    popup->move(topLeft);
    popup->show();
    popup->raise();
    searchEdit->setFocus(Qt::PopupFocusReason);

    const int generation = ++reloadGeneration;
    QTimer::singleShot(0, this, [this, generation]() {
        if (popup && popup->isVisible() && reloadGeneration == generation)
            emit modelsReloadRequested(this, generation);
    });
}

void ModelSelectBox::showLoading() {
    popupLoading = true;
    searchEdit->setEnabled(false);
    loadingLabel->setText(QStringLiteral("Loading..."));
    stack->setCurrentWidget(loadingPage);
}

void ModelSelectBox::rebuildRows() {
    if (!popup || popupLoading)
        return;

    proxyModel->setFilterText(searchEdit->text());
    updateCurrentIndexLater();
}

void ModelSelectBox::chooseModel(const QString &modelId) {
    hideTooltip();
    if (popup)
        popup->hide();

    if (selectedModelId == modelId)
        return;

    selectedModelId = modelId;
    updateButtonLabel();
    emit currentModelChanged(selectedModelId);
}

void ModelSelectBox::updateButtonLabel() {
    setText(selectedModelId.isEmpty() ? noSelectionLabel : selectedModelId);
    update();
}

void ModelSelectBox::showTooltipFor(const ModelInfo &model, const QRect &globalAnchor) {
    if (displayNameFor(model).isEmpty() || globalAnchor.isEmpty())
        return;

    if (!tooltip) {
        tooltip = new QFrame(nullptr, Qt::ToolTip | Qt::FramelessWindowHint);
        tooltip->setObjectName(QStringLiteral("modelInfoTooltip"));
        tooltip->setStyleSheet(QStringLiteral(
            "QFrame#modelInfoTooltip { background: #ffffe1; border: 1px solid #767676; }"
            "QLabel { color: #202020; }"
        ));
        auto *layout = new QVBoxLayout(tooltip);
        layout->setContentsMargins(8, 6, 8, 6);
        layout->setSpacing(4);
        tooltipName = new QLabel(tooltip);
        tooltipName->setWordWrap(true);
        QFont nameFont = tooltipName->font();
        nameFont.setBold(true);
        tooltipName->setFont(nameFont);
        tooltipDescription = new QLabel(tooltip);
        tooltipDescription->setWordWrap(true);
        tooltipPricing = new QLabel(tooltip);
        tooltipPricing->setWordWrap(true);
        tooltipKnowledge = new QLabel(tooltip);
        tooltipKnowledge->setWordWrap(true);
        layout->addWidget(tooltipName);
        layout->addWidget(tooltipDescription);
        layout->addWidget(tooltipPricing);
        layout->addWidget(tooltipKnowledge);
    }

    tooltipName->setText(richFieldText(QStringLiteral("Name"), model.name));
    tooltipDescription->setText(richFieldText(QStringLiteral("Description"), model.description));
    tooltipPricing->setText(richFieldText(QStringLiteral("Pricing"), pricingValueFor(model)));
    tooltipKnowledge->setText(richFieldText(QStringLiteral("Knowledge cutoff"), model.knowledgeCutoff));
    tooltip->setFixedWidth(kTooltipWidth);
    tooltip->adjustSize();

    QPoint pos(globalAnchor.right() + 8, globalAnchor.top());
    if (QScreen *screen = QGuiApplication::screenAt(pos)) {
        const QRect available = screen->availableGeometry();
        if (pos.x() + tooltip->width() > available.right())
            pos.setX(globalAnchor.left() - tooltip->width() - 8);
        if (pos.y() + tooltip->height() > available.bottom())
            pos.setY(available.bottom() - tooltip->height());
    }
    tooltip->move(pos);
    tooltip->show();
}

void ModelSelectBox::hideTooltip() {
    if (tooltip)
        tooltip->hide();
}

void ModelSelectBox::updateCurrentIndex() {
    if (!modelListView || !proxyModel)
        return;

    QModelIndex selectedIndex;
    for (int row = 0; row < proxyModel->rowCount(); ++row) {
        const QModelIndex index = proxyModel->index(row, 0);
        if (index.data(ModelIdRole).toString() == selectedModelId) {
            selectedIndex = index;
            break;
        }
    }

    if (!selectedIndex.isValid())
        selectedIndex = proxyModel->index(0, 0);

    modelListView->selectionModel()->setCurrentIndex(
        selectedIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    modelListView->doItemsLayout();

    const int viewportRows = qMax(1, modelListView->viewport()->height() / kRowHeight);
    const int centeredRow = qMax(0, selectedIndex.row() - viewportRows / 2);
    modelListView->verticalScrollBar()->setValue(centeredRow);
}

void ModelSelectBox::updateCurrentIndexLater() {
    QTimer::singleShot(0, this, [this]() {
        if (popup && popup->isVisible() && modelListView && stack->currentWidget() == modelListView)
            updateCurrentIndex();
    });
    QTimer::singleShot(25, this, [this]() {
        if (popup && popup->isVisible() && modelListView && stack->currentWidget() == modelListView)
            updateCurrentIndex();
    });
}

bool ModelSelectBox::isInfoArea(const QModelIndex &viewIndex, const QPoint &pos) const {
    if (!modelListView || !viewIndex.isValid() || viewIndex.data(IsEmptyRole).toBool())
        return false;

    return ModelListDelegate::infoRect(modelListView->visualRect(viewIndex)).contains(pos);
}

ModelInfo ModelSelectBox::modelForIndex(const QModelIndex &viewIndex) const {
    if (!proxyModel || !modelListModel || !viewIndex.isValid())
        return ModelInfo();

    const QModelIndex sourceIndex = proxyModel->mapToSource(viewIndex);
    return modelListModel->modelAtRow(sourceIndex.row());
}
