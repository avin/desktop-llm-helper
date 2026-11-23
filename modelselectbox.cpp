#include "modelselectbox.h"

#include <QFrame>
#include <QGuiApplication>
#include <QLabel>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace {
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

ModelSelectBox::ModelSelectBox(QWidget *parent)
    : SearchableSelectBox(tr("Search models"), parent, true) {
    setEmptyLabel(tr("Not selected"));
    connect(this, &SearchableSelectBox::currentValueChanged,
            this, &ModelSelectBox::currentModelChanged);
    connect(this, &ModelSelectBox::itemDetailsRequested, this,
            [this](const QString &id, const QRect &globalAnchor) {
        const auto model = modelsById.constFind(id);
        if (model != modelsById.cend())
            showTooltipFor(model.value(), globalAnchor);
    });
    connect(this, &ModelSelectBox::itemDetailsDismissed,
            this, &ModelSelectBox::hideTooltip);
}

QString ModelSelectBox::currentModel() const {
    return currentValue();
}

void ModelSelectBox::setCurrentModel(const QString &modelId) {
    setCurrentValue(modelId);
}

void ModelSelectBox::setEmptyLabel(const QString &label) {
    setEmptyChoiceLabel(label);
}

QString ModelSelectBox::emptyLabel() const {
    return emptyChoiceLabel();
}

int ModelSelectBox::currentReloadGeneration() const {
    return reloadGeneration;
}

void ModelSelectBox::setModels(const ModelInfoList &models) {
    ModelInfoList sorted = models;
    std::sort(sorted.begin(), sorted.end(), modelLessThan);

    QList<Item> items;
    items.reserve(sorted.size());
    modelsById.clear();
    for (const ModelInfo &model : sorted) {
        items.append(Item{model.id, displayNameFor(model), model.name});
        modelsById.insert(model.id, model);
    }
    setItems(std::move(items));
}

void ModelSelectBox::setLoadError(const QString &message) {
    showStatus(message.trimmed().isEmpty() ? tr("Failed to load models.") : message, true);
}

void ModelSelectBox::preparePopup() {
    modelsById.clear();
    setItems({});
    showStatus(tr("Loading..."), false);

    const int generation = ++reloadGeneration;
    QTimer::singleShot(0, this, [this, generation]() {
        if (isPopupVisible() && reloadGeneration == generation)
            emit modelsReloadRequested(this, generation);
    });
}

void ModelSelectBox::showTooltipFor(const ModelInfo &model, const QRect &globalAnchor) {
    if (displayNameFor(model).isEmpty() || globalAnchor.isEmpty())
        return;

    if (!tooltip) {
        tooltip = new QFrame(this, Qt::ToolTip | Qt::FramelessWindowHint);
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
