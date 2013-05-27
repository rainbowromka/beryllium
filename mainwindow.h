#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDir>
#include <QGst/Message>
#include <QGst/Pipeline>
#include <QGst/Element>
#include <QGst/Pad>
#include <QGst/Buffer>
#include <QGst/Ui/VideoWidget>

#include <QGlib/Error>
#include <QGlib/Value>

#include <QWidget>

QT_BEGIN_NAMESPACE
class QBoxLayout;
class QLabel;
class QListWidget;
class QMenuBar;
class QResizeEvent;
class QTimer;
class QToolBar;
class QToolButton;
class Worklist;
class ArchiveWindow;
class DcmDataset;
QT_END_NAMESPACE

class MainWindow : public QWidget
{
    Q_OBJECT

    // UI
    //
    QLabel*      lblRecordAll;
    QToolButton* btnStart;
    QToolButton* btnRecord;
    QToolButton* btnSnapshot;
    QAction*     actionSettings;
    ArchiveWindow* archiveWindow;
#ifdef WITH_DICOM
    Worklist*     worklist;
    QString       pendingSOPInstanceUID;
#endif
    QGst::Ui::VideoWidget* displayWidget;
    QListWidget*  imageList;
    QDir          outputPath;
    QString       pipelineDef;
    QString       patientName;
    QString       studyName;
    int           imageNo;
    int           clipNo;
    int           studyNo;

    QMenuBar* createMenuBar();
    QToolBar* createToolBar();
    void updateStartButton();
    void updateRecordButton();
    void updateRecordAll();

    // State machine
    //
    bool running;
    bool recording;

    // GStreamer pipeline
    //
    QGst::PipelinePtr pipeline;
    QGst::ElementPtr displaySink;
    QGst::ElementPtr imageValve;
    QGst::ElementPtr imageSink;
    QGst::ElementPtr clipValve;
    QGst::ElementPtr clipSink;
    QGst::ElementPtr videoEncoder;
    QGst::ElementPtr videoValve;
    QGst::ElementPtr videoSink;
    QGst::ElementPtr rtpSink;

    QString replace(QString str, int seqNo = 0);
    QString buildPipeline();
    QGst::PipelinePtr createPipeline();
    void releasePipeline();

    void onBusMessage(const QGst::MessagePtr& message);
    void onStateChangedMessage(const QGst::StateChangedMessagePtr& message);
    void onElementMessage(const QGst::ElementMessagePtr& message);

    void onImageReady(const QGst::BufferPtr&);
    void errorGlib(const QGlib::ObjectPtr& obj, const QGlib::Error& ex);
    void setElementProperty(const char* elm, const char* prop = nullptr, const QGlib::Value& value = nullptr);
    void setElementProperty(QGst::ElementPtr& elm, const char* prop = nullptr, const QGlib::Value& value = nullptr);

#ifdef WITH_DICOM
    void sendToServer(DcmDataset* dset, const QString& seriesUID);
#endif

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

protected:

private slots:
#ifdef WITH_DICOM
    void onShowWorkListClick();
#endif
    void onShowAboutClick();
    void onShowArchiveClick();
    void onShowSettingsClick();
    void onStartClick();
    void onSnapshotClick();
    void onRecordClick();
    void prepareSettingsMenu();
    void toggleSetting();
    void updatePipeline();
};

#endif // MAINWINDOW_H
