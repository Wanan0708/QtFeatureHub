#include "pdfviewer.h"
#include "ui_pdfviewer.h"

#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QFuture>
#include <QGraphicsOpacityEffect>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLinearGradient>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QEasingCurve>
#include <QPrinter>
#include <QPrintDialog>
#include <QPainter>
#include <QPixmap>
#include <QImage>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QStyle>
#include <QVariantAnimation>
#include <QWheelEvent>
#include <QCryptographicHash>
#include <QtConcurrent/QtConcurrentRun>
#include <QtMath>

#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-image.h>
#include <poppler/cpp/poppler-page.h>
#include <poppler/cpp/poppler-page-renderer.h>

namespace {

constexpr double kMinZoomFactor = 0.5;
constexpr double kMaxZoomFactor = 3.0;
constexpr double kZoomStep = 1.25;
constexpr double kPrintZoomFactor = 2.0;
constexpr int kMaxCachedPages = 6;
constexpr int kAllPagesBatchSize = 4;
constexpr double kThumbnailZoomFactor = 0.32;
constexpr int kThumbnailBatchSize = 8;
constexpr int kHighResPageRadius = 2;
constexpr int kAllPagesBatchDelayMs = 90;
constexpr int kPageFadeDurationMs = 140;
constexpr int kResumeHighResDelayMs = 180;
constexpr int kLoadingPulseDurationMs = 1800;
constexpr int kLoadingShimmerDurationMs = 2200;
constexpr int kFastScrollMinimumDelta = 24;
constexpr double kFastScrollVelocityThreshold = 1.25;
constexpr double kFastScrollSmoothedVelocityThreshold = 0.9;
constexpr double kFastScrollBurstVelocityThreshold = 2.4;
constexpr int kFastScrollEventThreshold = 2;
constexpr double kScrollVelocityBlendFactor = 0.35;
constexpr bool kEnableThumbnailRendering = true;
constexpr bool kEnableAdjacentPrefetch = false;
constexpr bool kEnableAllPagesBatchRendering = true;

QMutex &popplerRenderMutex()
{
    static QMutex mutex;
    return mutex;
}

QImage popplerImageToQImage(const poppler::image &image)
{
    if (!image.is_valid()) {
        return QImage();
    }

    QImage::Format format = QImage::Format_Invalid;
    switch (image.format()) {
    case poppler::image::format_mono:
        format = QImage::Format_Mono;
        break;
    case poppler::image::format_rgb24:
        format = QImage::Format_RGB888;
        break;
    case poppler::image::format_argb32:
        format = QImage::Format_ARGB32;
        break;
    case poppler::image::format_gray8:
        format = QImage::Format_Grayscale8;
        break;
    default:
        return QImage();
    }

    return QImage(reinterpret_cast<const uchar *>(image.const_data()),
                  image.width(),
                  image.height(),
                  image.bytes_per_row(),
                  format).copy();
}

QVector<int> parsePrintRanges(const QString &input, int pageCount, QString *errorMessage)
{
    QVector<int> pages;
    const QStringList segments = input.split(',', Qt::SkipEmptyParts);
    if (segments.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("请输入要打印的页码范围，例如 1,3-5");
        }
        return pages;
    }

    for (const QString &rawSegment : segments) {
        const QString segment = rawSegment.trimmed();
        if (segment.isEmpty()) {
            continue;
        }

        const QStringList bounds = segment.split('-', Qt::SkipEmptyParts);
        bool startOk = false;
        const int startPage = bounds.value(0).trimmed().toInt(&startOk);
        if (!startOk || startPage < 1 || startPage > pageCount) {
            if (errorMessage) {
                *errorMessage = QObject::tr("页码 %1 超出范围，允许范围是 1-%2").arg(segment).arg(pageCount);
            }
            return {};
        }

        int endPage = startPage;
        if (bounds.size() == 2) {
            bool endOk = false;
            endPage = bounds.at(1).trimmed().toInt(&endOk);
            if (!endOk || endPage < 1 || endPage > pageCount) {
                if (errorMessage) {
                    *errorMessage = QObject::tr("页码 %1 超出范围，允许范围是 1-%2").arg(segment).arg(pageCount);
                }
                return {};
            }
            if (endPage < startPage) {
                if (errorMessage) {
                    *errorMessage = QObject::tr("页码范围 %1 不合法，请使用从小到大的范围").arg(segment);
                }
                return {};
            }
        } else if (bounds.size() > 2) {
            if (errorMessage) {
                *errorMessage = QObject::tr("页码范围 %1 格式不正确").arg(segment);
            }
            return {};
        }

        for (int page = startPage; page <= endPage; ++page) {
            if (!pages.contains(page - 1)) {
                pages.append(page - 1);
            }
        }
    }

    return pages;
}

QPixmap renderLoadingPlaceholderPixmap(int placeholderWidth, int placeholderHeight, int pageIndex, bool compact, qreal shimmerPhase)
{
    QPixmap pixmap(placeholderWidth, placeholderHeight);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF cardRect(12.0, 12.0, placeholderWidth - 24.0, placeholderHeight - 24.0);
    const QRectF pageRect(cardRect.left() + 18.0, cardRect.top() + 18.0, cardRect.width() - 36.0, cardRect.height() - 72.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(245, 247, 250));
    painter.drawRoundedRect(cardRect, 12.0, 12.0);

    painter.setBrush(QColor(234, 238, 243));
    painter.drawRoundedRect(pageRect, 10.0, 10.0);

    painter.setBrush(QColor(222, 227, 233));
    const qreal lineWidth = pageRect.width() * 0.68;
    painter.drawRoundedRect(QRectF(pageRect.left() + 18.0, pageRect.top() + 22.0, lineWidth, 10.0), 5.0, 5.0);
    painter.drawRoundedRect(QRectF(pageRect.left() + 18.0, pageRect.top() + 48.0, pageRect.width() * 0.54, 10.0), 5.0, 5.0);
    painter.drawRoundedRect(QRectF(pageRect.left() + 18.0, pageRect.top() + 74.0, pageRect.width() * 0.74, 10.0), 5.0, 5.0);

    const qreal shimmerX = cardRect.left() - cardRect.width() * 0.35 + shimmerPhase * cardRect.width() * 1.7;
    QLinearGradient shimmerGradient(QPointF(shimmerX, cardRect.top()), QPointF(shimmerX + cardRect.width() * 0.42, cardRect.bottom()));
    shimmerGradient.setColorAt(0.0, QColor(255, 255, 255, 0));
    shimmerGradient.setColorAt(0.45, QColor(255, 255, 255, 0));
    shimmerGradient.setColorAt(0.5, QColor(255, 255, 255, 42));
    shimmerGradient.setColorAt(0.55, QColor(255, 255, 255, 0));
    shimmerGradient.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter.setBrush(shimmerGradient);
    painter.drawRoundedRect(cardRect, 12.0, 12.0);

    painter.setPen(QColor(123, 132, 145));
    QFont font = painter.font();
    font.setPointSize(compact ? 9 : 10);
    painter.setFont(font);
    painter.drawText(QRectF(cardRect.left() + 18.0, cardRect.bottom() - 38.0, cardRect.width() - 36.0, 18.0),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QObject::tr("第 %1 页").arg(pageIndex + 1));

    painter.setPen(QColor(154, 162, 173));
    painter.drawText(QRectF(cardRect.left() + 18.0, cardRect.bottom() - 20.0, cardRect.width() - 36.0, 16.0),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QObject::tr("加载中"));

    return pixmap;
}

PdfRenderResult renderPdfPage(const QString &filePath, int requestId, int pageIndex, double zoomFactor)
{
    PdfRenderResult result;
    result.requestId = requestId;
    result.pageIndex = pageIndex;
    result.zoomFactor = zoomFactor;

    QMutexLocker locker(&popplerRenderMutex());

    const QByteArray encodedFile = QFile::encodeName(filePath);
    std::unique_ptr<poppler::document> document(poppler::document::load_from_file(encodedFile.constData()));
    if (!document) {
        result.errorMessage = QObject::tr("无法重新打开当前 PDF 文件");
        return result;
    }

    if (pageIndex < 0 || pageIndex >= document->pages()) {
        result.errorMessage = QObject::tr("请求的页面超出范围");
        return result;
    }

    std::unique_ptr<poppler::page> page(document->create_page(pageIndex));
    if (!page) {
        result.errorMessage = QObject::tr("无法创建页面 %1").arg(pageIndex + 1);
        return result;
    }

    poppler::page_renderer renderer;
    renderer.set_render_hint(poppler::page_renderer::antialiasing, true);
    renderer.set_render_hint(poppler::page_renderer::text_antialiasing, true);
    const double dpi = 72.0 * qBound(kMinZoomFactor, zoomFactor, kMaxZoomFactor * 2.0);
    result.image = popplerImageToQImage(renderer.render_page(page.get(), dpi, dpi));
    if (result.image.isNull()) {
        result.errorMessage = QObject::tr("页面渲染失败");
    }

    return result;
}

QVector<PdfRenderResult> renderPdfPages(const QString &filePath, int requestId, const QVector<int> &pageIndexes, double zoomFactor)
{
    QVector<PdfRenderResult> results;
    if (pageIndexes.isEmpty()) {
        return results;
    }

    QMutexLocker locker(&popplerRenderMutex());

    const QByteArray encodedFile = QFile::encodeName(filePath);
    std::unique_ptr<poppler::document> document(poppler::document::load_from_file(encodedFile.constData()));
    if (!document) {
        PdfRenderResult result;
        result.requestId = requestId;
        result.errorMessage = QObject::tr("无法重新打开当前 PDF 文件");
        results.append(result);
        return results;
    }

    poppler::page_renderer renderer;
    renderer.set_render_hint(poppler::page_renderer::antialiasing, true);
    renderer.set_render_hint(poppler::page_renderer::text_antialiasing, true);
    const double dpi = 72.0 * qBound(kMinZoomFactor, zoomFactor, kMaxZoomFactor * 2.0);

    for (int pageIndex : pageIndexes) {
        PdfRenderResult result;
        result.requestId = requestId;
        result.pageIndex = pageIndex;
        result.zoomFactor = zoomFactor;

        if (pageIndex < 0 || pageIndex >= document->pages()) {
            result.errorMessage = QObject::tr("请求的页面超出范围");
            results.append(result);
            continue;
        }

        std::unique_ptr<poppler::page> page(document->create_page(pageIndex));
        if (!page) {
            result.errorMessage = QObject::tr("无法创建页面 %1").arg(pageIndex + 1);
            results.append(result);
            continue;
        }

        result.image = popplerImageToQImage(renderer.render_page(page.get(), dpi, dpi));
        if (result.image.isNull()) {
            result.errorMessage = QObject::tr("页面渲染失败");
        }
        results.append(result);
    }

    return results;
}

}

PdfViewer::PdfViewer(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PdfViewer)
{
    ui->setupUi(this);
    ui->scrollArea->setAlignment(Qt::AlignCenter);
    ui->scrollArea->viewport()->installEventFilter(this);
    ui->pageSpinBox->setKeyboardTracking(false);
    connect(&m_renderWatcher, &QFutureWatcher<PdfRenderResult>::finished, this, &PdfViewer::handleRenderFinished);
    connect(&m_prefetchWatcher, &QFutureWatcher<QVector<PdfRenderResult>>::finished, this, &PdfViewer::handlePrefetchFinished);
    connect(&m_allPagesWatcher, &QFutureWatcher<QVector<PdfRenderResult>>::finished, this, &PdfViewer::handleAllPagesBatchFinished);
    connect(&m_thumbnailWatcher, &QFutureWatcher<QVector<PdfRenderResult>>::finished, this, &PdfViewer::handleThumbnailBatchFinished);
    m_allPagesBatchTimer.setSingleShot(true);
    m_allPagesBatchTimer.setInterval(kAllPagesBatchDelayMs);
    connect(&m_allPagesBatchTimer, &QTimer::timeout, this, [this]() {
        requestAllPagesBatch();
    });
    m_resumeHighResTimer.setSingleShot(true);
    m_resumeHighResTimer.setInterval(kResumeHighResDelayMs);
    connect(&m_resumeHighResTimer, &QTimer::timeout, this, [this]() {
        m_suspendHighResRendering = false;
        if (m_showAllPages) {
            renderAllPages();
        }
    });
    connect(ui->scrollArea->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) {
        handleScrollPositionChanged(value);
    });
    ui->thumbnailList->setIconSize(QSize(80, 110));
    ui->thumbnailList->setSpacing(6);
    ui->thumbnailList->setUniformItemSizes(true);
    ui->thumbnailList->setSelectionMode(QAbstractItemView::NoSelection);
    ui->thumbnailList->setFocusPolicy(Qt::NoFocus);
    ui->thumbnailStatusLabel->setText(tr("缩略图导航"));
    connect(ui->thumbnailList, &QListWidget::currentRowChanged, this, &PdfViewer::on_thumbnailList_currentRowChanged);
    ui->label_image->setAlignment(Qt::AlignCenter);
    m_scrollAnimation = new QPropertyAnimation(ui->scrollArea->verticalScrollBar(), "value", this);
    m_scrollAnimation->setDuration(220);
    m_scrollAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_scrollAnimation, &QPropertyAnimation::finished, this, [this]() {
        m_ignoreScrollSync = false;
        syncCurrentPageFromScroll();
    });
    clearViewer(tr("请选择 PDF 文件"));
}

PdfViewer::~PdfViewer()
{
    delete ui;
}

void PdfViewer::on_openButton_clicked()
{
    const QString file = QFileDialog::getOpenFileName(this, tr("打开 PDF 文件"), QString(), tr("PDF Files (*.pdf);;All Files (*)"));
    if (file.isEmpty()) return;

    const QByteArray encodedFile = QFile::encodeName(file);
    std::unique_ptr<poppler::document> document(poppler::document::load_from_file(encodedFile.constData()));
    if (!document) {
        QMessageBox::warning(this, tr("打开失败"), tr("无法用 Poppler 打开此 PDF 文件：%1").arg(file));
        return;
    }

    m_doc = std::move(document);
    m_currentFile = file;
    m_currentPage = 0;
    m_manualZoomFactor = 1.0;
    m_fitToWidth = true;
    m_fitToPage = false;
    m_showAllPages = false;
    clearPageCache();
    rebuildThumbnailList();
    loadViewState();
    renderCurrentPage();
}

bool PdfViewer::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->scrollArea->viewport() && event->type() == QEvent::Wheel && m_doc) {
        QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(event);
        if (wheelEvent->modifiers().testFlag(Qt::ControlModifier)) {
            if (wheelEvent->angleDelta().y() > 0) {
                on_zoomInButton_clicked();
            } else if (wheelEvent->angleDelta().y() < 0) {
                on_zoomOutButton_clicked();
            }
            wheelEvent->accept();
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void PdfViewer::renderCurrentPage()
{
    if (!m_doc) {
        clearViewer(tr("请选择 PDF 文件"));
        return;
    }

    if (m_currentPage < 0) m_currentPage = 0;
    if (m_doc->pages() <= 0) {
        clearViewer(tr("当前 PDF 不包含可渲染页面"));
        return;
    }
    if (m_currentPage >= m_doc->pages()) m_currentPage = m_doc->pages() - 1;

    poppler::rectf pageRect;
    {
        QMutexLocker locker(&popplerRenderMutex());
        std::unique_ptr<poppler::page> p(m_doc->create_page(m_currentPage));
        if (!p) {
            QMessageBox::warning(this, tr("渲染失败"), tr("无法创建页面 %1").arg(m_currentPage + 1));
            return;
        }
        pageRect = p->page_rect();
    }

    double zoomFactor = m_manualZoomFactor;
    const QSize availableViewportSize = availablePageViewportSize(m_fitToWidth);
    if (m_fitToWidth && pageRect.width() > 0.0) {
        const int availableWidth = qMax(160, availableViewportSize.width());
        zoomFactor = availableWidth / pageRect.width();
    } else if (m_fitToPage && pageRect.width() > 0.0 && pageRect.height() > 0.0) {
        const int availableWidth = qMax(160, availableViewportSize.width());
        const int availableHeight = qMax(160, availableViewportSize.height());
        zoomFactor = qMin(availableWidth / pageRect.width(), availableHeight / pageRect.height());
    }
    zoomFactor = qBound(kMinZoomFactor, zoomFactor, kMaxZoomFactor);
    m_effectiveZoomFactor = zoomFactor;
    updateNavigationState();
    updateThumbnailSelection();
    saveViewState();

    if (m_showAllPages) {
        renderAllPages();
        return;
    }

    const QImage img = renderPageImage(m_currentPage, m_effectiveZoomFactor);
    if (!img.isNull()) {
        const QPixmap pix = QPixmap::fromImage(img);
        ui->label_image->show();
        applyPixmapToPageLabel(ui->label_image, pix, QStringLiteral("highres"), false);
        updateThumbnailForPage(m_currentPage);
        hideExtraPageLabels(1);
        requestThumbnailBatch();
        requestAdjacentPrefetch();
        return;
    }

    hideExtraPageLabels(1);
    ui->label_image->show();
    applyLoadingPlaceholder(ui->label_image, m_currentPage, false);
    requestPageRender(m_currentPage, m_effectiveZoomFactor);
}

void PdfViewer::renderAllPages(bool ensureCurrentVisible)
{
    if (!m_doc || m_doc->pages() <= 0) {
        clearViewer(tr("当前 PDF 不包含可渲染页面"));
        return;
    }

    trimDistantPageCache();
    const QPair<int, int> highResRange = preferredHighResPageRange();

    const int pageCount = m_doc->pages();
    for (int pageIndex = 0; pageIndex < pageCount; ++pageIndex) {
        QLabel *label = pageLabelAt(pageIndex);
        const QImage image = renderPageImage(pageIndex, m_effectiveZoomFactor);
        label->show();
        label->setAlignment(Qt::AlignCenter);
        const bool keepHighRes = highResRange.first >= 0
            && pageIndex >= highResRange.first
            && pageIndex <= highResRange.second;

        if (image.isNull()) {
            const auto thumbnailIt = m_thumbnailCache.constFind(pageIndex);
            if (thumbnailIt != m_thumbnailCache.constEnd() && (!keepHighRes || m_suspendHighResRendering)) {
                const QPixmap preview = thumbnailIt.value().scaled(220, 320, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                applyPixmapToPageLabel(label, preview, QStringLiteral("thumbnail"), false);
                label->setToolTip(tr("第 %1 页缩略预览").arg(pageIndex + 1));
            } else {
                applyLoadingPlaceholder(label, pageIndex, true);
                label->setToolTip(QString());
            }
            continue;
        }

        const QPixmap pix = QPixmap::fromImage(image);
        const bool animateFade = m_showAllPages && label->property("pageDisplayMode").toString() != QLatin1String("highres");
        applyPixmapToPageLabel(label, pix, QStringLiteral("highres"), animateFade);
        label->setToolTip(QString());
        updateThumbnailForPage(pageIndex);
    }

    hideExtraPageLabels(pageCount);
    if (ensureCurrentVisible) {
        scrollToPage(m_currentPage);
    }
    scheduleAllPagesBatch(ensureCurrentVisible);
}

void PdfViewer::on_prevButton_clicked()
{
    if (!m_doc) return;
    if (m_currentPage > 0) {
        --m_currentPage;
        if (m_showAllPages) {
            updateNavigationState();
            saveViewState();
            scrollToPage(m_currentPage);
            return;
        }
        renderCurrentPage();
    }
}

void PdfViewer::on_nextButton_clicked()
{
    if (!m_doc) return;
    if (m_currentPage < m_doc->pages() - 1) {
        ++m_currentPage;
        if (m_showAllPages) {
            updateNavigationState();
            saveViewState();
            scrollToPage(m_currentPage);
            return;
        }
        renderCurrentPage();
    }
}

void PdfViewer::on_printButton_clicked()
{
    if (!m_doc || m_doc->pages() <= 0) return;

    QMessageBox scopeDialog(this);
    scopeDialog.setWindowTitle(tr("打印范围"));
    scopeDialog.setText(tr("请选择打印范围"));
    QAbstractButton *currentPageButton = scopeDialog.addButton(tr("当前页"), QMessageBox::AcceptRole);
    QAbstractButton *allPagesButton = scopeDialog.addButton(tr("全部页面"), QMessageBox::AcceptRole);
    QAbstractButton *customRangeButton = scopeDialog.addButton(tr("自定义范围"), QMessageBox::ActionRole);
    scopeDialog.addButton(QMessageBox::Cancel);
    scopeDialog.setDefaultButton(qobject_cast<QPushButton *>(currentPageButton));
    scopeDialog.exec();

    if (scopeDialog.clickedButton() != currentPageButton
        && scopeDialog.clickedButton() != allPagesButton
        && scopeDialog.clickedButton() != customRangeButton) {
        return;
    }

    QVector<int> pagesToPrint;
    if (scopeDialog.clickedButton() == currentPageButton) {
        pagesToPrint.append(m_currentPage);
    } else if (scopeDialog.clickedButton() == allPagesButton) {
        for (int pageIndex = 0; pageIndex < m_doc->pages(); ++pageIndex) {
            pagesToPrint.append(pageIndex);
        }
    } else {
        bool ok = false;
        const QString defaultRange = QString::number(m_currentPage + 1);
        const QString rangeText = QInputDialog::getText(this,
                                                        tr("自定义打印范围"),
                                                        tr("请输入页码范围，例如 1,3-5"),
                                                        QLineEdit::Normal,
                                                        defaultRange,
                                                        &ok);
        if (!ok) {
            return;
        }

        QString errorMessage;
        pagesToPrint = parsePrintRanges(rangeText, m_doc->pages(), &errorMessage);
        if (pagesToPrint.isEmpty()) {
            QMessageBox::warning(this, tr("打印范围无效"), errorMessage);
            return;
        }
    }

    QPrinter printer;
    QPrintDialog dlg(&printer, this);
    if (dlg.exec() != QDialog::Accepted) return;

    QPainter painter(&printer);
    for (int index = 0; index < pagesToPrint.size(); ++index) {
        const int pageIndex = pagesToPrint.at(index);
        const PdfRenderResult renderResult = renderPdfPage(m_currentFile, 0, pageIndex, kPrintZoomFactor);
        const QImage image = renderResult.image;
        if (image.isNull()) {
            QMessageBox::warning(this,
                                 tr("打印失败"),
                                 renderResult.errorMessage.isEmpty()
                                     ? tr("页面 %1 渲染失败，打印已中止").arg(pageIndex + 1)
                                     : renderResult.errorMessage);
            painter.end();
            return;
        }

        const QPixmap pagePixmap = QPixmap::fromImage(image);
        const QRect pageRect = printer.pageRect();
        const QPixmap scaledPixmap = pagePixmap.scaled(pageRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        const QRect target((pageRect.width() - scaledPixmap.width()) / 2,
                           (pageRect.height() - scaledPixmap.height()) / 2,
                           scaledPixmap.width(),
                           scaledPixmap.height());
        painter.drawPixmap(target, scaledPixmap);

        if (index + 1 < pagesToPrint.size()) {
            printer.newPage();
        }
    }

    painter.end();
}

void PdfViewer::on_pageSpinBox_valueChanged(int pageNumber)
{
    if (!m_doc) return;

    const int targetPage = pageNumber - 1;
    if (targetPage == m_currentPage || targetPage < 0 || targetPage >= m_doc->pages()) {
        return;
    }

    m_currentPage = targetPage;
    if (m_showAllPages) {
        updateNavigationState();
        saveViewState();
        scrollToPage(m_currentPage);
        return;
    }
    renderCurrentPage();
}

void PdfViewer::on_zoomOutButton_clicked()
{
    setManualZoomFactor((m_fitToWidth ? m_effectiveZoomFactor : m_manualZoomFactor) / kZoomStep);
}

void PdfViewer::on_zoomInButton_clicked()
{
    setManualZoomFactor((m_fitToWidth ? m_effectiveZoomFactor : m_manualZoomFactor) * kZoomStep);
}

void PdfViewer::on_resetZoomButton_clicked()
{
    setManualZoomFactor(1.0);
}

void PdfViewer::on_fitWidthButton_toggled(bool checked)
{
    if (!m_doc) {
        ui->fitWidthButton->setChecked(false);
        return;
    }

    m_fitToWidth = checked;
    if (checked) {
        m_fitToPage = false;
        ui->fitPageButton->blockSignals(true);
        ui->fitPageButton->setChecked(false);
        ui->fitPageButton->blockSignals(false);
    } else if (!m_fitToPage) {
        m_manualZoomFactor = m_effectiveZoomFactor;
    }
    renderCurrentPage();
}

void PdfViewer::on_fitPageButton_toggled(bool checked)
{
    if (!m_doc) {
        ui->fitPageButton->setChecked(false);
        return;
    }

    m_fitToPage = checked;
    if (checked) {
        m_fitToWidth = false;
        ui->fitWidthButton->blockSignals(true);
        ui->fitWidthButton->setChecked(false);
        ui->fitWidthButton->blockSignals(false);
    } else if (!m_fitToWidth) {
        m_manualZoomFactor = m_effectiveZoomFactor;
    }
    renderCurrentPage();
}

void PdfViewer::on_showAllPagesButton_toggled(bool checked)
{
    if (!m_doc) {
        ui->showAllPagesButton->setChecked(false);
        return;
    }

    m_showAllPages = checked;
    if (checked) {
        m_lastScrollValue = ui->scrollArea->verticalScrollBar()->value();
        m_scrollElapsedTimer.invalidate();
        m_smoothedScrollVelocity = 0.0;
        m_fastScrollEventCount = 0;
        m_suspendHighResRendering = false;
        renderAllPages(true);
        return;
    }

    m_resumeHighResTimer.stop();
    m_allPagesBatchTimer.stop();
    m_smoothedScrollVelocity = 0.0;
    m_fastScrollEventCount = 0;
    m_suspendHighResRendering = false;
    renderCurrentPage();
}

void PdfViewer::on_thumbnailList_currentRowChanged(int currentRow)
{
    if (!m_doc || currentRow < 0 || currentRow >= m_doc->pages() || currentRow == m_currentPage) {
        return;
    }

    m_currentPage = currentRow;
    if (m_showAllPages) {
        updateNavigationState();
        saveViewState();
        scrollToPage(m_currentPage);
        return;
    }

    renderCurrentPage();
}

void PdfViewer::handleScrollPositionChanged(int value)
{
    if (m_ignoreScrollSync) {
        m_lastScrollValue = value;
        m_scrollElapsedTimer.restart();
        return;
    }

    const int delta = qAbs(value - m_lastScrollValue);
    const qint64 elapsedMs = m_scrollElapsedTimer.isValid() ? qMax<qint64>(1, m_scrollElapsedTimer.elapsed()) : -1;
    m_lastScrollValue = value;
    m_scrollElapsedTimer.restart();

    if (m_showAllPages) {
        const double instantVelocity = (elapsedMs > 0 && delta > 0) ? static_cast<double>(delta) / static_cast<double>(elapsedMs) : 0.0;
        if (m_smoothedScrollVelocity <= 0.0) {
            m_smoothedScrollVelocity = instantVelocity;
        } else {
            m_smoothedScrollVelocity = (1.0 - kScrollVelocityBlendFactor) * m_smoothedScrollVelocity
                + kScrollVelocityBlendFactor * instantVelocity;
        }

        const bool fastEvent = delta >= kFastScrollMinimumDelta
            && (instantVelocity >= kFastScrollVelocityThreshold
                || m_smoothedScrollVelocity >= kFastScrollSmoothedVelocityThreshold);
        const bool burstEvent = instantVelocity >= kFastScrollBurstVelocityThreshold;

        if (fastEvent) {
            ++m_fastScrollEventCount;
        } else {
            m_fastScrollEventCount = qMax(0, m_fastScrollEventCount - 1);
        }

        if (burstEvent || m_fastScrollEventCount >= kFastScrollEventThreshold) {
            if (!m_suspendHighResRendering) {
                m_suspendHighResRendering = true;
                m_allPagesBatchTimer.stop();
                renderAllPages();
            }
            m_resumeHighResTimer.start();
        } else {
            m_resumeHighResTimer.start();
        }
    }

    syncCurrentPageFromScroll();
}

void PdfViewer::scrollToPage(int pageIndex)
{
    if (!m_showAllPages || !m_doc || pageIndex < 0 || pageIndex >= m_doc->pages()) {
        return;
    }

    QLabel *label = pageLabelAt(pageIndex);
    if (!label) {
        return;
    }

    QScrollBar *scrollBar = ui->scrollArea->verticalScrollBar();
    const int targetValue = qBound(scrollBar->minimum(),
                                   label->y() + label->height() / 2 - ui->scrollArea->viewport()->height() / 2,
                                   scrollBar->maximum());

    m_ignoreScrollSync = true;
    m_scrollAnimation->stop();
    m_scrollAnimation->setStartValue(scrollBar->value());
    m_scrollAnimation->setEndValue(targetValue);
    m_scrollAnimation->start();
}

void PdfViewer::syncCurrentPageFromScroll()
{
    if (!m_showAllPages || !m_doc || m_doc->pages() <= 0 || m_ignoreScrollSync) {
        return;
    }

    const int viewportCenter = ui->scrollArea->verticalScrollBar()->value() + ui->scrollArea->viewport()->height() / 2;
    int bestPage = m_currentPage;
    int bestDistance = std::numeric_limits<int>::max();

    for (int pageIndex = 0; pageIndex < m_doc->pages(); ++pageIndex) {
        QLabel *label = pageLabelAt(pageIndex);
        if (!label || !label->isVisible()) {
            continue;
        }

        const int labelCenter = label->y() + label->height() / 2;
        const int distance = qAbs(labelCenter - viewportCenter);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestPage = pageIndex;
        }
    }

    if (bestPage != m_currentPage) {
        m_currentPage = bestPage;
        updateNavigationState();
        saveViewState();
    }

    scheduleAllPagesBatch();
}

void PdfViewer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    const bool sizeChanged = event->size().width() != event->oldSize().width()
        || event->size().height() != event->oldSize().height();
    if (m_doc && sizeChanged && (m_fitToWidth || m_fitToPage)) {
        renderCurrentPage();
    }
}

QSize PdfViewer::availablePageViewportSize(bool reserveVerticalScrollbar) const
{
    QSize availableSize = ui->scrollArea->viewport()->size();
    const QMargins margins = ui->verticalLayout_2->contentsMargins();
    availableSize.rwidth() -= margins.left() + margins.right();
    availableSize.rheight() -= margins.top() + margins.bottom();

    if (reserveVerticalScrollbar && !ui->scrollArea->verticalScrollBar()->isVisible()) {
        availableSize.rwidth() -= style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, ui->scrollArea);
    }

    availableSize.rwidth() -= 8;
    availableSize.rheight() -= 8;
    return availableSize;
}

QImage PdfViewer::renderPageImage(int pageIndex, double zoomFactor) const
{
    if (!m_doc || pageIndex < 0 || pageIndex >= m_doc->pages()) {
        return QImage();
    }

    const QString cacheKey = cacheKeyForPage(pageIndex, zoomFactor);
    const auto cacheIt = m_pageCache.constFind(cacheKey);
    if (cacheIt != m_pageCache.constEnd()) {
        rememberCacheKey(cacheKey);
        return cacheIt.value();
    }

    return QImage();
}

void PdfViewer::applyPixmapToPageLabel(QLabel *label, const QPixmap &pixmap, const QString &displayMode, bool animateFade)
{
    if (!label) {
        return;
    }

    stopLoadingPlaceholderAnimation(label);

    label->clear();
    label->setPixmap(pixmap);
    label->resize(pixmap.size());
    label->setMinimumSize(pixmap.size());
    label->setProperty("pageDisplayMode", displayMode);
    if (displayMode != QLatin1String("loading")) {
        label->setProperty("loadingPlaceholderKey", QString());
    }

    if (QGraphicsOpacityEffect *existingEffect = qobject_cast<QGraphicsOpacityEffect *>(label->graphicsEffect())) {
        label->setGraphicsEffect(nullptr);
    }

    if (!animateFade) {
        return;
    }

    auto *effect = new QGraphicsOpacityEffect(label);
    effect->setOpacity(0.0);
    label->setGraphicsEffect(effect);

    auto *animation = new QPropertyAnimation(effect, "opacity", label);
    animation->setDuration(kPageFadeDurationMs);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(animation, &QPropertyAnimation::finished, label, [label, effect]() {
        if (label->graphicsEffect() == effect) {
            label->setGraphicsEffect(nullptr);
            return;
        }

        if (effect && effect->parent() == label) {
            effect->deleteLater();
        }
    });
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void PdfViewer::applyLoadingPlaceholder(QLabel *label, int pageIndex, bool compact)
{
    if (!label) {
        return;
    }

    const int viewportWidth = ui->scrollArea->viewport()->width();
    const int placeholderWidth = compact
        ? qBound(220, viewportWidth - 72, 320)
        : qBound(320, viewportWidth - 48, 520);
    const int placeholderHeight = qRound(placeholderWidth * 1.35);
    const QString placeholderKey = QStringLiteral("%1:%2:%3")
        .arg(pageIndex)
        .arg(compact ? 1 : 0)
        .arg(placeholderWidth);

    if (label->property("pageDisplayMode").toString() == QLatin1String("loading")
        && label->property("loadingPlaceholderKey").toString() == placeholderKey) {
        ensureLoadingPlaceholderAnimation(label);
        return;
    }

    applyPixmapToPageLabel(label,
                           renderLoadingPlaceholderPixmap(placeholderWidth, placeholderHeight, pageIndex, compact, 0.18),
                           QStringLiteral("loading"),
                           false);
    label->setProperty("loadingPlaceholderKey", placeholderKey);
    label->setProperty("loadingPlaceholderPageIndex", pageIndex);
    label->setProperty("loadingPlaceholderCompact", compact);
    label->setProperty("loadingPlaceholderWidth", placeholderWidth);
    label->setProperty("loadingPlaceholderHeight", placeholderHeight);
    ensureLoadingPlaceholderAnimation(label);
}

void PdfViewer::stopLoadingPlaceholderAnimation(QLabel *label)
{
    if (!label) {
        return;
    }

    if (QPropertyAnimation *animation = label->findChild<QPropertyAnimation *>(QStringLiteral("loadingPulseAnimation"))) {
        animation->stop();
        animation->deleteLater();
    }

    if (QVariantAnimation *animation = label->findChild<QVariantAnimation *>(QStringLiteral("loadingShimmerAnimation"))) {
        animation->stop();
        animation->deleteLater();
    }

    if (label->property("pageDisplayMode").toString() != QLatin1String("loading")) {
        label->setProperty("loadingPlaceholderKey", QString());
    }
}

void PdfViewer::ensureLoadingPlaceholderAnimation(QLabel *label)
{
    if (!label) {
        return;
    }

    QGraphicsOpacityEffect *effect = qobject_cast<QGraphicsOpacityEffect *>(label->graphicsEffect());
    if (!effect) {
        effect = new QGraphicsOpacityEffect(label);
        effect->setOpacity(0.92);
        label->setGraphicsEffect(effect);
    }

    QPropertyAnimation *animation = label->findChild<QPropertyAnimation *>(QStringLiteral("loadingPulseAnimation"));
    if (!animation) {
        animation = new QPropertyAnimation(effect, "opacity", label);
        animation->setObjectName(QStringLiteral("loadingPulseAnimation"));
        animation->setDuration(kLoadingPulseDurationMs);
        animation->setLoopCount(-1);
        animation->setEasingCurve(QEasingCurve::InOutSine);
        animation->setKeyValueAt(0.0, 0.9);
        animation->setKeyValueAt(0.5, 1.0);
        animation->setKeyValueAt(1.0, 0.9);
    }

    if (animation->state() != QAbstractAnimation::Running) {
        animation->start();
    }

    QVariantAnimation *shimmerAnimation = label->findChild<QVariantAnimation *>(QStringLiteral("loadingShimmerAnimation"));
    if (!shimmerAnimation) {
        shimmerAnimation = new QVariantAnimation(label);
        shimmerAnimation->setObjectName(QStringLiteral("loadingShimmerAnimation"));
        shimmerAnimation->setDuration(kLoadingShimmerDurationMs);
        shimmerAnimation->setLoopCount(-1);
        shimmerAnimation->setStartValue(0.0);
        shimmerAnimation->setEndValue(1.0);
        shimmerAnimation->setEasingCurve(QEasingCurve::InOutSine);
        connect(shimmerAnimation, &QVariantAnimation::valueChanged, label, [this, label](const QVariant &value) {
            if (!label || label->property("pageDisplayMode").toString() != QLatin1String("loading")) {
                return;
            }

            const int pageIndex = label->property("loadingPlaceholderPageIndex").toInt();
            const bool compact = label->property("loadingPlaceholderCompact").toBool();
            const int placeholderWidth = label->property("loadingPlaceholderWidth").toInt();
            const int placeholderHeight = label->property("loadingPlaceholderHeight").toInt();
            if (placeholderWidth <= 0 || placeholderHeight <= 0) {
                return;
            }

            label->setPixmap(renderLoadingPlaceholderPixmap(placeholderWidth,
                                                            placeholderHeight,
                                                            pageIndex,
                                                            compact,
                                                            value.toReal()));
        });
    }

    if (shimmerAnimation->state() != QAbstractAnimation::Running) {
        shimmerAnimation->start();
    }
}

QLabel *PdfViewer::pageLabelAt(int pageIndex)
{
    if (pageIndex == 0) {
        return ui->label_image;
    }

    const int extraIndex = pageIndex - 1;
    while (m_extraPageLabels.size() <= extraIndex) {
        QLabel *label = new QLabel(ui->scrollAreaWidgetContents);
        label->setAlignment(Qt::AlignCenter);
        label->setMinimumSize(320, 240);
        ui->verticalLayout_2->addWidget(label);
        m_extraPageLabels.append(label);
    }

    return m_extraPageLabels.at(extraIndex);
}

QLabel *PdfViewer::existingPageLabelAt(int pageIndex) const
{
    if (pageIndex == 0) {
        return ui->label_image;
    }

    const int extraIndex = pageIndex - 1;
    if (extraIndex < 0 || extraIndex >= m_extraPageLabels.size()) {
        return nullptr;
    }

    return m_extraPageLabels.at(extraIndex);
}

void PdfViewer::hideExtraPageLabels(int visiblePageCount)
{
    if (visiblePageCount <= 1) {
        for (QLabel *label : m_extraPageLabels) {
            label->hide();
        }
        return;
    }

    for (int index = 0; index < m_extraPageLabels.size(); ++index) {
        m_extraPageLabels.at(index)->setVisible(index < visiblePageCount - 1);
    }
}

QPair<int, int> PdfViewer::visiblePageRange() const
{
    if (!m_doc || m_doc->pages() <= 0) {
        return qMakePair(-1, -1);
    }

    if (!m_showAllPages) {
        return qMakePair(m_currentPage, m_currentPage);
    }

    const QScrollBar *scrollBar = ui->scrollArea->verticalScrollBar();
    const int viewportTop = scrollBar->value();
    const int viewportBottom = viewportTop + ui->scrollArea->viewport()->height();
    int firstVisiblePage = -1;
    int lastVisiblePage = -1;

    for (int pageIndex = 0; pageIndex < m_doc->pages(); ++pageIndex) {
        QLabel *label = existingPageLabelAt(pageIndex);
        if (!label || !label->isVisible()) {
            continue;
        }

        const int labelTop = label->y();
        const int labelBottom = labelTop + label->height();
        if (labelBottom < viewportTop || labelTop > viewportBottom) {
            continue;
        }

        if (firstVisiblePage < 0) {
            firstVisiblePage = pageIndex;
        }
        lastVisiblePage = pageIndex;
    }

    if (firstVisiblePage < 0 || lastVisiblePage < 0) {
        return qMakePair(m_currentPage, m_currentPage);
    }

    return qMakePair(firstVisiblePage, lastVisiblePage);
}

bool PdfViewer::shouldKeepHighResPage(int pageIndex) const
{
    const QPair<int, int> highResRange = preferredHighResPageRange();
    return highResRange.first >= 0 && pageIndex >= highResRange.first && pageIndex <= highResRange.second;
}

QPair<int, int> PdfViewer::preferredHighResPageRange() const
{
    if (!m_doc || m_doc->pages() <= 0) {
        return qMakePair(-1, -1);
    }

    const int fallbackFirst = qMax(0, m_currentPage - kHighResPageRadius);
    const int fallbackLast = qMin(m_doc->pages() - 1, m_currentPage + kHighResPageRadius);
    if (!m_showAllPages) {
        return qMakePair(fallbackFirst, fallbackLast);
    }

    const QPair<int, int> visibleRange = visiblePageRange();
    const int firstVisiblePage = visibleRange.first;
    const int lastVisiblePage = visibleRange.second;

    if (firstVisiblePage < 0 || lastVisiblePage < 0) {
        return qMakePair(fallbackFirst, fallbackLast);
    }

    return qMakePair(qMax(0, firstVisiblePage - 1), qMin(m_doc->pages() - 1, lastVisiblePage + 1));
}

QVector<int> PdfViewer::prioritizedVisiblePages() const
{
    QVector<int> pageIndexes;
    if (!m_doc || m_doc->pages() <= 0) {
        return pageIndexes;
    }

    const QPair<int, int> highResRange = preferredHighResPageRange();
    if (highResRange.first < 0) {
        return pageIndexes;
    }

    int anchorPage = m_currentPage;
    if (m_showAllPages) {
        const QScrollBar *scrollBar = ui->scrollArea->verticalScrollBar();
        const int viewportCenter = scrollBar->value() + ui->scrollArea->viewport()->height() / 2;
        int bestDistance = std::numeric_limits<int>::max();

        for (int pageIndex = highResRange.first; pageIndex <= highResRange.second; ++pageIndex) {
            QLabel *label = existingPageLabelAt(pageIndex);
            if (!label || !label->isVisible()) {
                continue;
            }

            const int labelCenter = label->y() + label->height() / 2;
            const int distance = qAbs(labelCenter - viewportCenter);
            if (distance < bestDistance) {
                bestDistance = distance;
                anchorPage = pageIndex;
            }
        }
    }

    auto appendIfMissing = [&](int pageIndex) {
        if (pageIndex < highResRange.first || pageIndex > highResRange.second || pageIndexes.contains(pageIndex)) {
            return;
        }
        pageIndexes.append(pageIndex);
    };

    appendIfMissing(anchorPage);
    for (int offset = 1; offset <= highResRange.second - highResRange.first; ++offset) {
        appendIfMissing(anchorPage - offset);
        appendIfMissing(anchorPage + offset);
    }

    return pageIndexes;
}

void PdfViewer::trimDistantPageCache()
{
    if (!m_showAllPages || !m_doc) {
        return;
    }

    const QPair<int, int> highResRange = preferredHighResPageRange();
    QStringList keysToRemove;
    for (const QString &cacheKey : m_cacheOrder) {
        bool ok = false;
        const int pageIndex = cacheKey.section(QLatin1Char('@'), 0, 0).toInt(&ok);
        if (!ok) {
            continue;
        }

        if (highResRange.first < 0 || pageIndex < highResRange.first || pageIndex > highResRange.second) {
            keysToRemove.append(cacheKey);
        }
    }

    for (const QString &cacheKey : keysToRemove) {
        m_cacheOrder.removeAll(cacheKey);
        m_pageCache.remove(cacheKey);
    }
}

void PdfViewer::requestPageRender(int pageIndex, double zoomFactor)
{
    if (m_currentFile.isEmpty()) {
        return;
    }

    if (m_renderWatcher.isRunning()) {
        m_hasPendingRender = true;
        m_pendingRenderPage = pageIndex;
        m_pendingRenderZoomFactor = zoomFactor;
        return;
    }

    startPageRender(pageIndex, zoomFactor);
}

void PdfViewer::scheduleAllPagesBatch(bool immediate)
{
    if (!kEnableAllPagesBatchRendering) {
        m_allPagesBatchTimer.stop();
        return;
    }

    if (!m_showAllPages || !m_doc || m_currentFile.isEmpty()) {
        m_allPagesBatchTimer.stop();
        return;
    }

    if (m_suspendHighResRendering) {
        if (!m_resumeHighResTimer.isActive()) {
            m_resumeHighResTimer.start();
        }
        return;
    }

    if (immediate) {
        m_allPagesBatchTimer.stop();
        requestAllPagesBatch();
        return;
    }

    m_allPagesBatchTimer.start();
}

void PdfViewer::startPageRender(int pageIndex, double zoomFactor)
{
    m_hasPendingRender = false;

    ++m_renderRequestId;
    const int requestId = m_renderRequestId;
    QFuture<PdfRenderResult> future = QtConcurrent::run(renderPdfPage, m_currentFile, requestId, pageIndex, zoomFactor);
    m_renderWatcher.setFuture(future);
}

void PdfViewer::handleRenderFinished()
{
    const PdfRenderResult result = m_renderWatcher.result();
    if (result.requestId != m_renderRequestId) {
        if (m_hasPendingRender) {
            startPageRender(m_pendingRenderPage, m_pendingRenderZoomFactor);
        }
        return;
    }

    if (result.image.isNull()) {
        QMessageBox::warning(this, tr("渲染失败"), result.errorMessage.isEmpty() ? tr("页面渲染失败") : result.errorMessage);
        m_doc.reset();
        m_currentFile.clear();
        m_currentPage = 0;
        m_showAllPages = false;
        clearViewer(tr("页面渲染失败"));
        return;
    }

    const QString cacheKey = cacheKeyForPage(result.pageIndex, result.zoomFactor);
    m_pageCache.insert(cacheKey, result.image);
    rememberCacheKey(cacheKey);
    updateThumbnailForPage(result.pageIndex);
    requestThumbnailBatch();

    if (m_showAllPages) {
        renderAllPages();
    }

    if (!m_showAllPages && result.pageIndex == m_currentPage && qFuzzyCompare(result.zoomFactor + 1.0, m_effectiveZoomFactor + 1.0)) {
        const QPixmap pix = QPixmap::fromImage(result.image);
        ui->label_image->show();
        applyPixmapToPageLabel(ui->label_image, pix, QStringLiteral("highres"), false);
        hideExtraPageLabels(1);
        requestAdjacentPrefetch();
    }

    if (m_hasPendingRender) {
        startPageRender(m_pendingRenderPage, m_pendingRenderZoomFactor);
    }
}

void PdfViewer::requestAllPagesBatch()
{
    if (!kEnableAllPagesBatchRendering) {
        return;
    }

    if (!m_showAllPages || !m_doc || m_currentFile.isEmpty()) {
        return;
    }

    if (m_allPagesWatcher.isRunning()) {
        m_hasPendingAllPagesBatch = true;
        m_pendingAllPagesZoomFactor = m_effectiveZoomFactor;
        return;
    }

    QVector<int> pageIndexes;
    const int pageCount = m_doc->pages();
    const QVector<int> prioritizedPages = prioritizedVisiblePages();
    auto appendIfMissing = [&](int pageIndex) {
        if (pageIndex < 0 || pageIndex >= pageCount || pageIndexes.contains(pageIndex)) {
            return;
        }

        if (renderPageImage(pageIndex, m_effectiveZoomFactor).isNull()) {
            pageIndexes.append(pageIndex);
        }
    };

    for (int pageIndex : prioritizedPages) {
        if (pageIndexes.size() >= kAllPagesBatchSize) {
            break;
        }
        appendIfMissing(pageIndex);
    }

    if (pageIndexes.isEmpty()) {
        return;
    }

    startAllPagesBatch(pageIndexes, m_effectiveZoomFactor);
}

void PdfViewer::startAllPagesBatch(const QVector<int> &pageIndexes, double zoomFactor)
{
    m_hasPendingAllPagesBatch = false;
    ++m_allPagesRequestId;
    const int requestId = m_allPagesRequestId;
    QFuture<QVector<PdfRenderResult>> future = QtConcurrent::run(renderPdfPages, m_currentFile, requestId, pageIndexes, zoomFactor);
    m_allPagesWatcher.setFuture(future);
}

void PdfViewer::handleAllPagesBatchFinished()
{
    const QVector<PdfRenderResult> results = m_allPagesWatcher.result();
    if (results.isEmpty()) {
        if (m_hasPendingAllPagesBatch) {
            scheduleAllPagesBatch();
        }
        return;
    }

    if (results.first().requestId != m_allPagesRequestId) {
        if (m_hasPendingAllPagesBatch) {
            scheduleAllPagesBatch();
        }
        return;
    }

    for (const PdfRenderResult &result : results) {
        if (result.image.isNull()) {
            continue;
        }

        const QString cacheKey = cacheKeyForPage(result.pageIndex, result.zoomFactor);
        m_pageCache.insert(cacheKey, result.image);
        rememberCacheKey(cacheKey);
        updateThumbnailForPage(result.pageIndex);
    }

    if (m_showAllPages) {
        renderAllPages();
    }

    if (m_hasPendingAllPagesBatch) {
        scheduleAllPagesBatch();
    }
}

void PdfViewer::requestThumbnailBatch()
{
    if (!kEnableThumbnailRendering) {
        return;
    }

    if (!m_doc || m_currentFile.isEmpty() || ui->thumbnailList->count() <= 0) {
        return;
    }

    if (m_thumbnailWatcher.isRunning()) {
        m_hasPendingThumbnailBatch = true;
        return;
    }

    QVector<int> pageIndexes;
    const int pageCount = m_doc->pages();
    auto appendIfMissing = [&](int pageIndex) {
        if (pageIndex < 0 || pageIndex >= pageCount || pageIndexes.contains(pageIndex) || m_thumbnailCache.contains(pageIndex)) {
            return;
        }
        pageIndexes.append(pageIndex);
    };

    for (int offset = 0; offset < pageCount && pageIndexes.size() < kThumbnailBatchSize; ++offset) {
        appendIfMissing(m_currentPage - offset);
        if (pageIndexes.size() >= kThumbnailBatchSize) {
            break;
        }
        if (offset > 0) {
            appendIfMissing(m_currentPage + offset);
        }
    }

    for (int pageIndex = 0; pageIndex < pageCount && pageIndexes.size() < kThumbnailBatchSize; ++pageIndex) {
        appendIfMissing(pageIndex);
    }

    if (pageIndexes.isEmpty()) {
        return;
    }

    startThumbnailBatch(pageIndexes);
}

void PdfViewer::startThumbnailBatch(const QVector<int> &pageIndexes)
{
    m_hasPendingThumbnailBatch = false;
    ++m_thumbnailRequestId;
    const int requestId = m_thumbnailRequestId;
    QFuture<QVector<PdfRenderResult>> future = QtConcurrent::run(renderPdfPages, m_currentFile, requestId, pageIndexes, kThumbnailZoomFactor);
    m_thumbnailWatcher.setFuture(future);
}

void PdfViewer::handleThumbnailBatchFinished()
{
    const QVector<PdfRenderResult> results = m_thumbnailWatcher.result();
    if (results.isEmpty()) {
        if (m_hasPendingThumbnailBatch) {
            requestThumbnailBatch();
        }
        return;
    }

    if (results.first().requestId != m_thumbnailRequestId) {
        if (m_hasPendingThumbnailBatch) {
            requestThumbnailBatch();
        }
        return;
    }

    for (const PdfRenderResult &result : results) {
        if (result.image.isNull()) {
            continue;
        }

        const QPixmap thumbnail = QPixmap::fromImage(result.image).scaled(ui->thumbnailList->iconSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_thumbnailCache.insert(result.pageIndex, thumbnail);
        updateThumbnailForPage(result.pageIndex);
    }

    if (m_hasPendingThumbnailBatch) {
        requestThumbnailBatch();
        return;
    }

    requestThumbnailBatch();
}

void PdfViewer::requestAdjacentPrefetch()
{
    if (!kEnableAdjacentPrefetch) {
        return;
    }

    if (!m_doc || m_currentFile.isEmpty()) {
        return;
    }

    QVector<int> pageIndexes;
    const int previousPage = m_currentPage - 1;
    const int nextPage = m_currentPage + 1;
    if (previousPage >= 0 && renderPageImage(previousPage, m_effectiveZoomFactor).isNull()) {
        pageIndexes.append(previousPage);
    }
    if (nextPage < m_doc->pages() && renderPageImage(nextPage, m_effectiveZoomFactor).isNull()) {
        if (!pageIndexes.contains(nextPage)) {
            pageIndexes.append(nextPage);
        }
    }

    if (pageIndexes.isEmpty()) {
        return;
    }

    if (m_prefetchWatcher.isRunning()) {
        m_hasPendingPrefetch = true;
        m_pendingPrefetchPages = pageIndexes;
        m_pendingPrefetchZoomFactor = m_effectiveZoomFactor;
        return;
    }

    startPrefetchPages(pageIndexes, m_effectiveZoomFactor);
}

void PdfViewer::startPrefetchPages(const QVector<int> &pageIndexes, double zoomFactor)
{
    m_hasPendingPrefetch = false;
    ++m_prefetchRequestId;
    const int requestId = m_prefetchRequestId;
    QFuture<QVector<PdfRenderResult>> future = QtConcurrent::run(renderPdfPages, m_currentFile, requestId, pageIndexes, zoomFactor);
    m_prefetchWatcher.setFuture(future);
}

void PdfViewer::handlePrefetchFinished()
{
    const QVector<PdfRenderResult> results = m_prefetchWatcher.result();
    if (results.isEmpty()) {
        if (m_hasPendingPrefetch) {
            startPrefetchPages(m_pendingPrefetchPages, m_pendingPrefetchZoomFactor);
        }
        return;
    }

    if (results.first().requestId != m_prefetchRequestId) {
        if (m_hasPendingPrefetch) {
            startPrefetchPages(m_pendingPrefetchPages, m_pendingPrefetchZoomFactor);
        }
        return;
    }

    for (const PdfRenderResult &result : results) {
        if (result.image.isNull()) {
            continue;
        }

        const QString cacheKey = cacheKeyForPage(result.pageIndex, result.zoomFactor);
        m_pageCache.insert(cacheKey, result.image);
        rememberCacheKey(cacheKey);
        updateThumbnailForPage(result.pageIndex);
    }

    if (m_hasPendingPrefetch) {
        startPrefetchPages(m_pendingPrefetchPages, m_pendingPrefetchZoomFactor);
    }
}

void PdfViewer::clearPageCache()
{
    ++m_renderRequestId;
    ++m_prefetchRequestId;
    ++m_allPagesRequestId;
    ++m_thumbnailRequestId;
    m_hasPendingRender = false;
    m_hasPendingPrefetch = false;
    m_hasPendingAllPagesBatch = false;
    m_hasPendingThumbnailBatch = false;
    m_allPagesBatchTimer.stop();
    m_resumeHighResTimer.stop();
    m_suspendHighResRendering = false;
    m_lastScrollValue = 0;
    m_scrollElapsedTimer.invalidate();
    m_smoothedScrollVelocity = 0.0;
    m_fastScrollEventCount = 0;
    m_pendingPrefetchPages.clear();
    m_pageCache.clear();
    m_cacheOrder.clear();
    m_thumbnailCache.clear();
}

QString PdfViewer::cacheKeyForPage(int pageIndex, double zoomFactor) const
{
    return QString::number(pageIndex) + QLatin1Char('@') + QString::number(qRound(zoomFactor * 1000.0));
}

void PdfViewer::rememberCacheKey(const QString &cacheKey) const
{
    m_cacheOrder.removeAll(cacheKey);
    m_cacheOrder.append(cacheKey);

    int maxCacheEntries = kMaxCachedPages;
    if (m_showAllPages && m_doc) {
        const QPair<int, int> highResRange = preferredHighResPageRange();
        const int visibleSpan = highResRange.first >= 0 ? (highResRange.second - highResRange.first + 1) : kMaxCachedPages;
        maxCacheEntries = qMax(kMaxCachedPages * 2, visibleSpan + 4);
    }

    while (m_cacheOrder.size() > maxCacheEntries) {
        const QString oldestKey = m_cacheOrder.takeFirst();
        m_pageCache.remove(oldestKey);
    }
}

void PdfViewer::clearViewer(const QString &message)
{
    clearPageCache();
    rebuildThumbnailList();
    hideExtraPageLabels(1);
    stopLoadingPlaceholderAnimation(ui->label_image);
    ui->label_image->setProperty("pageDisplayMode", QString());
    ui->label_image->setProperty("loadingPlaceholderKey", QString());
    if (QGraphicsOpacityEffect *existingEffect = qobject_cast<QGraphicsOpacityEffect *>(ui->label_image->graphicsEffect())) {
        ui->label_image->setGraphicsEffect(nullptr);
    }
    ui->label_image->show();
    ui->label_image->setPixmap(QPixmap());
    ui->label_image->setMinimumSize(320, 240);
    ui->label_image->resize(ui->label_image->minimumSize());
    ui->label_image->setText(message);
    m_effectiveZoomFactor = m_manualZoomFactor;
    updateNavigationState();
}

void PdfViewer::loadViewState()
{
    if (m_currentFile.isEmpty() || !m_doc || m_doc->pages() <= 0) {
        return;
    }

    const QString settingsKey = settingsKeyForCurrentFile();
    QSettings settings(QStringLiteral("QtFeatureHub"), QStringLiteral("QtFeatureHub"));
    settings.beginGroup(QStringLiteral("pdfViewer/%1").arg(settingsKey));

    m_currentPage = qBound(0, settings.value(QStringLiteral("page"), 0).toInt(), m_doc->pages() - 1);
    m_manualZoomFactor = qBound(kMinZoomFactor,
                                settings.value(QStringLiteral("manualZoom"), 1.0).toDouble(),
                                kMaxZoomFactor);
    m_fitToWidth = settings.value(QStringLiteral("fitToWidth"), true).toBool();
    m_fitToPage = settings.value(QStringLiteral("fitToPage"), false).toBool();
    m_showAllPages = false;
    if (m_fitToWidth && m_fitToPage) {
        m_fitToPage = false;
    }

    settings.endGroup();

    ui->fitWidthButton->blockSignals(true);
    ui->fitWidthButton->setChecked(m_fitToWidth);
    ui->fitWidthButton->blockSignals(false);
    ui->fitPageButton->blockSignals(true);
    ui->fitPageButton->setChecked(m_fitToPage);
    ui->fitPageButton->blockSignals(false);
    ui->showAllPagesButton->blockSignals(true);
    ui->showAllPagesButton->setChecked(m_showAllPages);
    ui->showAllPagesButton->blockSignals(false);
    updateThumbnailSelection();
}

void PdfViewer::saveViewState() const
{
    if (m_currentFile.isEmpty() || !m_doc || m_doc->pages() <= 0) {
        return;
    }

    const QString settingsKey = settingsKeyForCurrentFile();
    QSettings settings(QStringLiteral("QtFeatureHub"), QStringLiteral("QtFeatureHub"));
    settings.beginGroup(QStringLiteral("pdfViewer/%1").arg(settingsKey));
    settings.setValue(QStringLiteral("filePath"), QFileInfo(m_currentFile).absoluteFilePath());
    settings.setValue(QStringLiteral("page"), m_currentPage);
    settings.setValue(QStringLiteral("manualZoom"), m_manualZoomFactor);
    settings.setValue(QStringLiteral("fitToWidth"), m_fitToWidth);
    settings.setValue(QStringLiteral("fitToPage"), m_fitToPage);
    settings.setValue(QStringLiteral("showAllPages"), m_showAllPages);
    settings.endGroup();
}

QString PdfViewer::settingsKeyForCurrentFile() const
{
    const QByteArray fileHash = QCryptographicHash::hash(QFileInfo(m_currentFile).absoluteFilePath().toUtf8(),
                                                         QCryptographicHash::Sha1);
    return QString::fromLatin1(fileHash.toHex());
}

void PdfViewer::rebuildThumbnailList()
{
    QSignalBlocker blocker(ui->thumbnailList);
    ui->thumbnailList->clear();

    if (!m_doc || m_doc->pages() <= 0) {
        ui->thumbnailList->setEnabled(false);
        ui->thumbnailStatusLabel->setText(tr("缩略图导航\n暂无文档"));
        return;
    }

    ui->thumbnailList->setEnabled(true);
    for (int pageIndex = 0; pageIndex < m_doc->pages(); ++pageIndex) {
        QListWidgetItem *item = new QListWidgetItem(tr("第 %1 页").arg(pageIndex + 1));
        item->setTextAlignment(Qt::AlignCenter);
        ui->thumbnailList->addItem(item);
        updateThumbnailForPage(pageIndex);
    }

    updateThumbnailSelection();
    refreshThumbnailViewportState();
}

void PdfViewer::refreshThumbnailViewportState()
{
    if (!m_doc || m_doc->pages() <= 0) {
        ui->thumbnailStatusLabel->setText(tr("缩略图导航\n暂无文档"));
        return;
    }

    const QPair<int, int> visibleRange = visiblePageRange();
    const QColor currentBackground(102, 163, 255);
    const QColor currentForeground(Qt::white);
    const QColor defaultForeground = ui->thumbnailList->palette().color(QPalette::Text);

    QString statusText;
    if (m_showAllPages && visibleRange.first >= 0 && visibleRange.second >= visibleRange.first) {
        statusText = visibleRange.first == visibleRange.second
            ? tr("可视页: %1\n当前页: %2").arg(visibleRange.first + 1).arg(m_currentPage + 1)
            : tr("可视页: %1-%2\n当前页: %3")
                  .arg(visibleRange.first + 1)
                  .arg(visibleRange.second + 1)
                  .arg(m_currentPage + 1);
    } else {
        statusText = tr("当前页: %1 / %2").arg(m_currentPage + 1).arg(m_doc->pages());
    }
    ui->thumbnailStatusLabel->setText(statusText);

    for (int pageIndex = 0; pageIndex < ui->thumbnailList->count(); ++pageIndex) {
        QListWidgetItem *item = ui->thumbnailList->item(pageIndex);
        if (!item) {
            continue;
        }

        const bool isCurrent = pageIndex == m_currentPage;
        const bool isVisible = visibleRange.first >= 0 && pageIndex >= visibleRange.first && pageIndex <= visibleRange.second;
        QFont font = item->font();
        font.setBold(isCurrent);
        item->setFont(font);

        if (isCurrent) {
            item->setBackground(currentBackground);
            item->setForeground(currentForeground);
        } else {
            item->setBackground(QBrush());
            item->setForeground(defaultForeground);
        }

        QString toolTip = tr("第 %1 页").arg(pageIndex + 1);
        if (isCurrent && isVisible && m_showAllPages) {
            toolTip += tr("\n当前页，且在可视区域内");
        } else if (isCurrent) {
            toolTip += tr("\n当前页");
        } else if (isVisible) {
            toolTip += tr("\n当前可视区域");
        }
        item->setToolTip(toolTip);
    }
}

void PdfViewer::updateThumbnailSelection()
{
    QSignalBlocker blocker(ui->thumbnailList);
    if (!m_doc || m_doc->pages() <= 0 || m_currentPage < 0 || m_currentPage >= ui->thumbnailList->count()) {
        ui->thumbnailList->clearSelection();
        ui->thumbnailList->setCurrentRow(-1);
        return;
    }

    ui->thumbnailList->setCurrentRow(m_currentPage);
    ui->thumbnailList->clearSelection();
    if (QListWidgetItem *item = ui->thumbnailList->item(m_currentPage)) {
        ui->thumbnailList->scrollToItem(item, QAbstractItemView::PositionAtCenter);
    }
    refreshThumbnailViewportState();
}

void PdfViewer::updateThumbnailForPage(int pageIndex)
{
    if (!m_doc || pageIndex < 0 || pageIndex >= ui->thumbnailList->count()) {
        return;
    }

    QListWidgetItem *item = ui->thumbnailList->item(pageIndex);
    if (!item) {
        return;
    }

    const auto thumbnailIt = m_thumbnailCache.constFind(pageIndex);
    if (thumbnailIt == m_thumbnailCache.constEnd()) {
        item->setIcon(QIcon());
        return;
    }

    item->setIcon(QIcon(thumbnailIt.value()));
}

void PdfViewer::updateNavigationState()
{
    const bool hasDocument = m_doc && m_doc->pages() > 0;
    const int pageCount = hasDocument ? m_doc->pages() : 0;

    ui->prevButton->setEnabled(hasDocument && m_currentPage > 0);
    ui->nextButton->setEnabled(hasDocument && m_currentPage + 1 < pageCount);
    ui->printButton->setEnabled(hasDocument);
    ui->pageSpinBox->setEnabled(hasDocument);
    ui->zoomOutButton->setEnabled(hasDocument);
    ui->zoomInButton->setEnabled(hasDocument);
    ui->resetZoomButton->setEnabled(hasDocument);
    ui->fitWidthButton->setEnabled(hasDocument);
    ui->fitPageButton->setEnabled(hasDocument);
    ui->showAllPagesButton->setEnabled(hasDocument);

    ui->pageSpinBox->blockSignals(true);
    ui->pageSpinBox->setMinimum(1);
    ui->pageSpinBox->setMaximum(qMax(1, pageCount));
    ui->pageSpinBox->setValue(hasDocument ? m_currentPage + 1 : 1);
    ui->pageSpinBox->blockSignals(false);

    ui->totalPagesLabel->setText(QString("/ %1").arg(pageCount));
    ui->fitWidthButton->blockSignals(true);
    ui->fitWidthButton->setChecked(hasDocument && m_fitToWidth);
    ui->fitWidthButton->blockSignals(false);
    ui->fitPageButton->blockSignals(true);
    ui->fitPageButton->setChecked(hasDocument && m_fitToPage);
    ui->fitPageButton->blockSignals(false);
    ui->showAllPagesButton->blockSignals(true);
    ui->showAllPagesButton->setChecked(hasDocument && m_showAllPages);
    ui->showAllPagesButton->blockSignals(false);

    if (!hasDocument) {
        ui->zoomValueLabel->setText(tr("--%"));
        return;
    }

    const int zoomPercent = qRound(m_effectiveZoomFactor * 100.0);
    if (m_fitToWidth) {
        ui->zoomValueLabel->setText(tr("适宽 %1%").arg(zoomPercent));
    } else if (m_fitToPage) {
        ui->zoomValueLabel->setText(tr("整页 %1%").arg(zoomPercent));
    } else {
        ui->zoomValueLabel->setText(tr("%1%").arg(zoomPercent));
    }
}

void PdfViewer::setManualZoomFactor(double zoomFactor)
{
    if (!m_doc) return;

    m_fitToWidth = false;
    m_fitToPage = false;
    m_manualZoomFactor = qBound(kMinZoomFactor, zoomFactor, kMaxZoomFactor);
    renderCurrentPage();
}
