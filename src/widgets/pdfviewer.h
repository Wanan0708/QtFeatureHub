#pragma once

#include <QEvent>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QHash>
#include <QImage>
#include <QLabel>
#include <QPair>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>
#include <QWidget>
#include <memory>

namespace poppler { class document; }

namespace Ui { class PdfViewer; }

struct PdfRenderResult
{
    int requestId = 0;
    int pageIndex = -1;
    double zoomFactor = 1.0;
    QImage image;
    QString errorMessage;
};

class PdfViewer : public QWidget
{
    Q_OBJECT

public:
    explicit PdfViewer(QWidget *parent = nullptr);
    ~PdfViewer();

private slots:
    void on_openButton_clicked();
    void on_printButton_clicked();
    void on_prevButton_clicked();
    void on_nextButton_clicked();
    void on_pageSpinBox_valueChanged(int pageNumber);
    void on_zoomOutButton_clicked();
    void on_zoomInButton_clicked();
    void on_resetZoomButton_clicked();
    void on_fitWidthButton_toggled(bool checked);
    void on_fitPageButton_toggled(bool checked);
    void on_showAllPagesButton_toggled(bool checked);
    void on_thumbnailList_currentRowChanged(int currentRow);

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void renderCurrentPage();
    void renderAllPages(bool ensureCurrentVisible = false);
    void handleScrollPositionChanged(int value);
    void scrollToPage(int pageIndex);
    void syncCurrentPageFromScroll();
    void requestPageRender(int pageIndex, double zoomFactor);
    void startPageRender(int pageIndex, double zoomFactor);
    void handleRenderFinished();
    void scheduleAllPagesBatch(bool immediate = false);
    void requestAllPagesBatch();
    void startAllPagesBatch(const QVector<int> &pageIndexes, double zoomFactor);
    void handleAllPagesBatchFinished();
    void requestAdjacentPrefetch();
    void startPrefetchPages(const QVector<int> &pageIndexes, double zoomFactor);
    void handlePrefetchFinished();
    void requestThumbnailBatch();
    void startThumbnailBatch(const QVector<int> &pageIndexes);
    void handleThumbnailBatchFinished();
    QSize availablePageViewportSize(bool reserveVerticalScrollbar) const;
    QImage renderPageImage(int pageIndex, double zoomFactor) const;
    void applyPixmapToPageLabel(QLabel *label, const QPixmap &pixmap, const QString &displayMode, bool animateFade);
    void applyLoadingPlaceholder(QLabel *label, int pageIndex, bool compact);
    void stopLoadingPlaceholderAnimation(QLabel *label);
    void ensureLoadingPlaceholderAnimation(QLabel *label);
    QLabel *pageLabelAt(int pageIndex);
    QLabel *existingPageLabelAt(int pageIndex) const;
    void hideExtraPageLabels(int visiblePageCount);
    QPair<int, int> visiblePageRange() const;
    QPair<int, int> preferredHighResPageRange() const;
    QVector<int> prioritizedVisiblePages() const;
    bool shouldKeepHighResPage(int pageIndex) const;
    void trimDistantPageCache();
    void clearPageCache();
    QString cacheKeyForPage(int pageIndex, double zoomFactor) const;
    void rememberCacheKey(const QString &cacheKey) const;
    void clearViewer(const QString &message);
    void loadViewState();
    void saveViewState() const;
    QString settingsKeyForCurrentFile() const;
    void rebuildThumbnailList();
    void refreshThumbnailViewportState();
    void updateThumbnailSelection();
    void updateThumbnailForPage(int pageIndex);
    void updateNavigationState();
    void setManualZoomFactor(double zoomFactor);

    Ui::PdfViewer *ui;
    std::unique_ptr<poppler::document> m_doc;
    int m_currentPage = 0;
    QString m_currentFile;
    double m_manualZoomFactor = 1.0;
    double m_effectiveZoomFactor = 1.0;
    bool m_fitToWidth = true;
    bool m_fitToPage = false;
    bool m_showAllPages = false;
    mutable QHash<QString, QImage> m_pageCache;
    mutable QStringList m_cacheOrder;
    QVector<QLabel *> m_extraPageLabels;
    QFutureWatcher<PdfRenderResult> m_renderWatcher;
    int m_renderRequestId = 0;
    bool m_hasPendingRender = false;
    int m_pendingRenderPage = -1;
    double m_pendingRenderZoomFactor = 1.0;
    QFutureWatcher<QVector<PdfRenderResult>> m_prefetchWatcher;
    int m_prefetchRequestId = 0;
    bool m_hasPendingPrefetch = false;
    QVector<int> m_pendingPrefetchPages;
    double m_pendingPrefetchZoomFactor = 1.0;
    QFutureWatcher<QVector<PdfRenderResult>> m_allPagesWatcher;
    int m_allPagesRequestId = 0;
    bool m_hasPendingAllPagesBatch = false;
    double m_pendingAllPagesZoomFactor = 1.0;
    bool m_ignoreScrollSync = false;
    QHash<int, QPixmap> m_thumbnailCache;
    QFutureWatcher<QVector<PdfRenderResult>> m_thumbnailWatcher;
    int m_thumbnailRequestId = 0;
    bool m_hasPendingThumbnailBatch = false;
    QTimer m_allPagesBatchTimer;
    QTimer m_resumeHighResTimer;
    QElapsedTimer m_scrollElapsedTimer;
    int m_lastScrollValue = 0;
    double m_smoothedScrollVelocity = 0.0;
    int m_fastScrollEventCount = 0;
    bool m_suspendHighResRendering = false;
    QPropertyAnimation *m_scrollAnimation = nullptr;
};
