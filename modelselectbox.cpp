#include "modelselectbox.h"

#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QStackedWidget>
#include <QStyle>
#include <QStyleOptionButton>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace {
constexpr int kPopupHeight = 300;
constexpr int kRowHeight = 30;
constexpr int kTooltipWidth = 320;

class ElidedLabel : public QLabel {
public:
    explicit ElidedLabel(QWidget *parent = nullptr)
        : QLabel(parent) {
        setMinimumWidth(0);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    }

    void setFullText(const QString &text) {
        fullTextValue = text;
        setToolTip(text);
        update();
    }

    QSize sizeHint() const override {
        QSize hint = QLabel::sizeHint();
        hint.setWidth(0);
        return hint;
    }

    QSize minimumSizeHint() const override {
        QSize hint = QLabel::minimumSizeHint();
        hint.setWidth(0);
        return hint;
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        Q_UNUSED(event);

        QPainter painter(this);
        const QString visibleText = fontMetrics().elidedText(fullTextValue, Qt::ElideRight, width());
        style()->drawItemText(&painter, rect(), alignment() | Qt::AlignVCenter,
                              palette(), isEnabled(), visibleText, foregroundRole());
    }

private:
    QString fullTextValue;
};

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
    rebuildRows();
    stack->setCurrentWidget(scrollArea);
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

    if (event->type() == QEvent::MouseButtonRelease && !watched->property("isModelHelp").toBool()) {
        const ModelInfo model = modelForObject(watched);
        if (!displayNameFor(model).isEmpty() || watched->property("isEmptyModelRow").toBool()) {
            chooseModel(model.id);
            return true;
        }
    }

    if (event->type() == QEvent::Enter && watched->property("isModelHelp").toBool()) {
        const ModelInfo model = modelForObject(watched);
        showTooltipFor(model, qobject_cast<QWidget *>(watched));
        return false;
    }

    if (event->type() == QEvent::Leave && watched->property("isModelHelp").toBool()) {
        hideTooltip();
        return false;
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
    connect(searchEdit, &QLineEdit::textChanged, this, &ModelSelectBox::rebuildRows);

    stack = new QStackedWidget(popup);
    layout->addWidget(stack, 1);

    loadingPage = new QWidget(stack);
    auto *loadingLayout = new QVBoxLayout(loadingPage);
    loadingLabel = new QLabel(QStringLiteral("Loading..."), loadingPage);
    loadingLabel->setAlignment(Qt::AlignCenter);
    loadingLayout->addWidget(loadingLabel, 1);
    stack->addWidget(loadingPage);

    scrollArea = new QScrollArea(stack);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rowsWidget = new QWidget(scrollArea);
    rowsWidget->setMinimumWidth(0);
    rowsWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    rowsLayout = new QVBoxLayout(rowsWidget);
    rowsLayout->setContentsMargins(0, 0, 0, 0);
    rowsLayout->setSpacing(0);
    scrollArea->setWidget(rowsWidget);
    stack->addWidget(scrollArea);
}

void ModelSelectBox::showPopup() {
    ensurePopup();
    hideTooltip();
    loadedModels.clear();
    searchEdit->clear();
    showLoading();

    const QPoint topLeft = mapToGlobal(QPoint(0, 0));
    popup->setFixedSize(width(), kPopupHeight);
    popup->move(topLeft);
    popup->show();
    popup->raise();
    searchEdit->setFocus(Qt::PopupFocusReason);

    const int generation = ++reloadGeneration;
    QTimer::singleShot(50, this, [this, generation]() {
        if (popup && popup->isVisible() && reloadGeneration == generation)
            emit modelsReloadRequested(this, generation);
    });
}

void ModelSelectBox::showLoading() {
    popupLoading = true;
    clearRows();
    searchEdit->setEnabled(false);
    loadingLabel->setText(QStringLiteral("Loading..."));
    stack->setCurrentWidget(loadingPage);
}

void ModelSelectBox::rebuildRows() {
    if (!popup || popupLoading)
        return;

    clearRows();

    ModelInfo emptyModel;
    addRow(emptyModel, true);

    const QString filter = searchEdit->text().trimmed().toLower();
    for (const ModelInfo &model : loadedModels) {
        const QString visibleName = displayNameFor(model);
        if (visibleName.isEmpty())
            continue;
        const bool matches = filter.isEmpty()
            || visibleName.toLower().contains(filter)
            || model.name.toLower().contains(filter);
        if (matches)
            addRow(model, false);
    }

    rowsLayout->addStretch(1);
    rowsWidget->adjustSize();

    QTimer::singleShot(0, this, [this]() {
        if (!scrollArea)
            return;

        QWidget *selectedRow = nullptr;
        const auto children = rowsWidget->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly);
        for (QWidget *child : children) {
            if (child->property("modelId").toString() == selectedModelId) {
                selectedRow = child;
                break;
            }
        }
        if (!selectedRow)
            return;

        const int viewportHeight = scrollArea->viewport()->height();
        const int target = selectedRow->y() - (viewportHeight - selectedRow->height()) / 2;
        scrollArea->verticalScrollBar()->setValue(qMax(0, target));
    });
}

void ModelSelectBox::addRow(const ModelInfo &model, bool isEmptyItem) {
    auto *row = new QFrame(rowsWidget);
    row->setProperty("modelId", model.id);
    row->setProperty("isEmptyModelRow", isEmptyItem);
    row->setCursor(Qt::PointingHandCursor);
    row->installEventFilter(this);
    row->setMinimumWidth(0);
    row->setMinimumHeight(kRowHeight);
    row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    const bool selected = isEmptyItem ? selectedModelId.isEmpty() : selectedModelId == model.id;
    row->setStyleSheet(selected
        ? QStringLiteral("QFrame { background: palette(highlight); } QLabel { color: palette(highlighted-text); }")
        : QStringLiteral("QFrame:hover { background: palette(alternate-base); }"));

    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(8, 0, 8, 0);
    layout->setSpacing(6);

    auto *nameLabel = new ElidedLabel(row);
    nameLabel->setFullText(isEmptyItem ? noSelectionLabel : displayNameFor(model));
    nameLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    nameLabel->setMinimumWidth(0);
    if (isEmptyItem && !selected) {
        QPalette pal = nameLabel->palette();
        pal.setColor(QPalette::WindowText, QColor(170, 170, 170));
        nameLabel->setPalette(pal);
    }
    layout->addWidget(nameLabel, 1);

    if (!isEmptyItem) {
        auto *helpLabel = new QLabel(QStringLiteral("?"), row);
        helpLabel->setAlignment(Qt::AlignCenter);
        helpLabel->setFixedSize(18, 18);
        helpLabel->setCursor(Qt::ArrowCursor);
        helpLabel->setProperty("isModelHelp", true);
        helpLabel->setProperty("modelId", model.id);
        helpLabel->installEventFilter(this);
        helpLabel->setStyleSheet(QStringLiteral(
            "QLabel { border: 1px solid palette(mid); border-radius: 9px; color: palette(mid); font-weight: bold; }"
            "QLabel:hover { border-color: palette(text); color: palette(text); }"
        ));
        layout->addWidget(helpLabel, 0, Qt::AlignVCenter);
    }

    rowsLayout->addWidget(row);
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

void ModelSelectBox::showTooltipFor(const ModelInfo &model, QWidget *anchor) {
    if (!anchor)
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

    QPoint pos = anchor->mapToGlobal(QPoint(anchor->width() + 8, 0));
    if (QScreen *screen = QGuiApplication::screenAt(pos)) {
        const QRect available = screen->availableGeometry();
        if (pos.x() + tooltip->width() > available.right())
            pos.setX(anchor->mapToGlobal(QPoint(-tooltip->width() - 8, 0)).x());
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

void ModelSelectBox::clearRows() {
    if (!rowsLayout)
        return;

    while (QLayoutItem *item = rowsLayout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->setParent(nullptr);
            widget->deleteLater();
        }
        delete item;
    }
}

ModelInfo ModelSelectBox::modelForObject(QObject *object) const {
    const QString modelId = object ? object->property("modelId").toString() : QString();
    if (modelId.isEmpty())
        return ModelInfo();

    const auto it = std::find_if(loadedModels.cbegin(), loadedModels.cend(),
                                 [&modelId](const ModelInfo &model) { return model.id == modelId; });
    return it == loadedModels.cend() ? ModelInfo() : *it;
}
