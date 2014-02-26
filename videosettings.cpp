/*
 * Copyright (C) 2013-2014 Irkutsk Diagnostic Center.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "videosettings.h"
#include "defaults.h"
#include <algorithm>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTextEdit>

// Video encoder
//
#include <QGlib/Error>
#include <QGlib/ParamSpec>
#include <QGlib/Value>
#include <QGst/Caps>
#include <QGst/ElementFactory>
#include <QGst/IntRange>
#include <QGst/Pad>
#include <QGst/Parse>
#include <QGst/Pipeline>
#include <QGst/PropertyProbe>
#include <QGst/Structure>
#include <gst/gst.h>
#include <gst/interfaces/tuner.h>

VideoSettings::VideoSettings(QWidget *parent) :
    QWidget(parent)
{
    QSettings settings;
    auto layout = new QFormLayout();

    layout->addRow(tr("Video &device"), listDevices = new QComboBox());
    connect(listDevices, SIGNAL(currentIndexChanged(int)), this, SLOT(videoDeviceChanged(int)));
    layout->addRow(tr("I&nput channel"), listChannels = new QComboBox());
    connect(listChannels, SIGNAL(currentIndexChanged(int)), this, SLOT(inputChannelChanged(int)));
    layout->addRow(tr("Pixel &format"), listFormats = new QComboBox());
    connect(listFormats, SIGNAL(currentIndexChanged(int)), this, SLOT(formatChanged(int)));
    layout->addRow(tr("Frame &size"), listSizes = new QComboBox());
    layout->addRow(tr("Video &codec"), listVideoCodecs = new QComboBox());
    connect(listVideoCodecs, SIGNAL(currentIndexChanged(int)), this, SLOT(videoCodecChanged(int)));

    auto elm = QGst::ElementFactory::make("videorate");
    if (elm && elm->findProperty("max-rate"))
    {
        layout->addRow(tr("M&ax rate"), spinFps = new QSpinBox());
        spinFps->setRange(0, 200);
        spinFps->setValue(settings.value("video-max-fps", DEFAULT_VIDEO_MAX_FPS).toInt());
        spinFps->setSuffix(tr(" frames per second"));
        spinFps->setToolTip(tr("0 for unlimited"));
    }
    layout->addRow(tr("Video &bitrate"), spinBitrate = new QSpinBox());
    spinBitrate->setRange(300, 102400);
    spinBitrate->setSingleStep(100);
    spinBitrate->setSuffix(tr(" kbit per second"));
    spinBitrate->setValue(settings.value("bitrate", DEFAULT_VIDEOBITRATE).toInt());

    layout->addRow(nullptr, checkDeinterlace = new QCheckBox(tr("De&interlace")));
    checkDeinterlace->setChecked(settings.value("video-deinterlace").toBool());

    layout->addRow(tr("Video &muxer"), listVideoMuxers = new QComboBox());
    layout->addRow(tr("&Image codec"), listImageCodecs = new QComboBox());
    layout->addRow(tr("RTP &payloader"), listRtpPayloaders = new QComboBox());
    textRtpClients = new QLineEdit(settings.value("rtp-clients").toString());
    layout->addRow(tr("&RTP clients"), textRtpClients);
    checkEnableRtp = new QCheckBox(tr("&Enable RTP"));
    checkEnableRtp->setChecked(settings.value("enable-rtp").toBool());
    layout->addRow(nullptr, checkEnableRtp);
    setLayout(layout);
}

void VideoSettings::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    // Refill the boxes every time the page is shown
    //
    auto selectedCodec = updateGstList("video-encoder", DEFAULT_VIDEO_ENCODER, GST_ELEMENT_FACTORY_TYPE_ENCODER | GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO, listVideoCodecs);
    listVideoCodecs->insertItem(0, tr("(none)"));
    if (selectedCodec.isEmpty())
    {
        listVideoCodecs->setCurrentIndex(0);
    }
    updateGstList("video-muxer",   DEFAULT_VIDEO_MUXER,   GST_ELEMENT_FACTORY_TYPE_MUXER, listVideoMuxers);
    updateGstList("image-encoder", DEFAULT_IMAGE_ENCODER, GST_ELEMENT_FACTORY_TYPE_ENCODER | GST_ELEMENT_FACTORY_TYPE_MEDIA_IMAGE, listImageCodecs);
    updateGstList("rtp-payloader", DEFAULT_RTP_PAYLOADER, GST_ELEMENT_FACTORY_TYPE_PAYLOADER, listRtpPayloaders);

    listDevices->clear();
    listDevices->addItem(tr("(default)"));

    // Populate cameras list
    //
    updateDeviceList(PLATFORM_SPECIFIC_SOURCE, PLATFORM_SPECIFIC_PROPERTY);

#ifndef Q_OS_WIN
    // Populate firewire list
    //
    updateDeviceList("dv1394src", "guid");
#endif
}

QString VideoSettings::updateGstList(const char* setting, const char* def, unsigned long long type, QComboBox* cb)
{
    cb->clear();
    auto selectedCodec = QSettings().value(setting, def).toString();

    auto elmList = gst_element_factory_list_get_elements(type, GST_RANK_NONE);
    for (auto curr = elmList; curr; curr = curr->next)
    {
        auto factory = QGst::ElementFactoryPtr::wrap(GST_ELEMENT_FACTORY(curr->data), true);
        cb->addItem(factory->longName(), factory->name());
        if (selectedCodec == factory->name())
        {
            cb->setCurrentIndex(cb->count() - 1);
        }
    }

    gst_plugin_feature_list_free(elmList);
    return selectedCodec;
}

void VideoSettings::updateDeviceList(const char* elmName, const char* propName)
{
    QSettings settings;
    auto selectedDevice = settings.value("device-type") == elmName? settings.value("device").toString(): nullptr;
    auto src = QGst::ElementFactory::make(elmName);
    if (!src) {
        QMessageBox::critical(this, windowTitle(), tr("Failed to create element '%1'").arg(elmName));
        return;
    }

    // Look for device-name for windows and "device" for linux/macosx
    //
    QGst::PropertyProbePtr propertyProbe = src.dynamicCast<QGst::PropertyProbe>();
    if (propertyProbe && propertyProbe->propertySupportsProbe(propName))
    {
        //get a list of devices that the element supports
        auto devices = propertyProbe->probeAndGetValues(propName);
        foreach (const QGlib::Value& device, devices)
        {
            auto deviceName = device.toString();
            auto srcPad = src->getStaticPad("src");
            if (srcPad)
            {
                // To set the property, the device must be in Null state
                //
                src->setProperty(propName, device);

                // To query the caps, the device must be in Ready state
                //
                src->setState(QGst::StateReady);
                src->getState(nullptr, nullptr, GST_SECOND * 10);

                //qDebug() << deviceName << " caps:\n" << srcPad->caps()->toString();
                QStringList channelsAndCaps;

                // First three entries will be device type, device id, caps
                //
                channelsAndCaps << elmName << deviceName << srcPad->caps()->toString();

                auto tuner = GST_TUNER(src);
                if (tuner)
                {
                    // The list is owned by the GstTuner and must not be freed.
                    //
                    auto channelList = gst_tuner_list_channels(tuner);
                    while (channelList)
                    {
                        auto ch = GST_TUNER_CHANNEL(channelList->data);
                        //gst_tuner_set_channel(tuner, ch);

                        channelsAndCaps.append(ch->label);
                        channelList = g_list_next(channelList);
                    }
                }
                else
                {
                    // Check for generic 'channel' property
                    //
                    QGlib::ParamSpecPtr channelSpec = src->findProperty("channel");
                    if (channelSpec && channelSpec->flags().testFlag(QGlib::ParamSpec::ReadWrite) &&
                        QGlib::Type::Int == channelSpec->valueType().fundamental())
                    {
                        for (int i = 1; i <= 64; ++i)
                        {
                            channelsAndCaps.append(QString::number(i));
                        }
                    }
                }

                auto friendlyName = src->property("device-name").toString();
                friendlyName = friendlyName.isEmpty()? deviceName: deviceName + " (" + friendlyName + ")";

                listDevices->addItem(friendlyName, channelsAndCaps);
                if (selectedDevice == deviceName)
                {
                    listDevices->setCurrentIndex(listDevices->count() - 1);
                }

                // Now switch the device back to Null state (release resources)
                //
                src->setState(QGst::StateNull);
                src->getState(nullptr, nullptr, GST_SECOND * 10);
            }
        }
    }
}

void VideoSettings::videoDeviceChanged(int index)
{
    listChannels->clear();

    auto channels = listDevices->itemData(index).toStringList();
    if (index < 0 || channels.size() < 3)
    {
        listChannels->addItem(tr("(default)"));
        return;
    }

    auto caps = channels.at(2);
    auto selectedChannel = QSettings().value("video-channel").toString();

    listChannels->addItem(tr("(default)"), caps);

    // From index 3 till end here is channels
    //
    for (int i = 3; i < channels.size(); ++i)
    {
        listChannels->addItem(channels.at(i), caps);
        if (channels.at(i) == selectedChannel)
        {
            listChannels->setCurrentIndex(listChannels->count() - 1);
        }
    }
}

static QStringList getFormats(const QGlib::Value& value)
{
    QStringList formats;

    if (GST_TYPE_LIST == value.type() || GST_TYPE_ARRAY == value.type())
    {
        for (uint idx = 0; idx < gst_value_list_get_size(value); ++idx)
        {
            auto elm = gst_value_list_get_value(value, idx);
            formats << QGlib::Value(elm).toString();
        }
    }
    else
    {
        formats << value.toString();
    }

    return formats;
}

void VideoSettings::inputChannelChanged(int index)
{
    listFormats->clear();
    listFormats->addItem(tr("(default)"));

    auto caps = QGst::Caps::fromString(listChannels->itemData(index).toString());

    if (index < 0 || !caps)
    {
        return;
    }

    auto selectedFormat = QSettings().value("format").toString();
    for (uint i = 0; i < caps->size(); ++i)
    {
        auto s = caps->internalStructure(i);
        Q_FOREACH (auto format, getFormats(s->value("format")))
        {
            auto formatName = format.isEmpty()? s->name(): s->name().append(",format=").append(format);
            if (listFormats->findData(formatName) >= 0)
            {
                continue;
            }
            auto displayName = format.isEmpty()? s->name(): s->name().append(" (").append(format).append(")");
            listFormats->addItem(displayName, formatName);
            if (formatName == selectedFormat)
            {
                listFormats->setCurrentIndex(listFormats->count() - 1);
            }
        }
    }
}

static QGst::IntRange getRange(const QGlib::Value& value)
{
    // First, try extract a single int value (more common)
    //
    bool ok = false;
    int intValue = value.toInt(&ok);
    if (ok)
    {
        return QGst::IntRange(intValue, intValue);
    }

    // If failed, try extract a range value
    //
    return ok? QGst::IntRange(intValue, intValue): value.get<QGst::IntRange>();
}

void VideoSettings::formatChanged(int index)
{
    listSizes->clear();
    listSizes->addItem(tr("(default)"));

    auto caps = QGst::Caps::fromString(listChannels->itemData(listChannels->currentIndex()).toString());
    auto selectedFormat = listFormats->itemData(index).toString();
    if (index < 0 || !caps || selectedFormat.isEmpty())
    {
        return;
    }

    QList<QSize> sizes;
    auto selectedSize = QSettings().value("size").toSize();
    for (uint i = 0; i < caps->size(); ++i)
    {
        auto s = caps->internalStructure(i);
        auto format = s->value("format").toString();
        auto formatName = format.isEmpty()? s->name(): s->name().append(",format=(fourcc)").append(format);
        if (selectedFormat != formatName)
        {
            continue;
        }

        QGst::IntRange widthRange = getRange(s->value("width"));
        QGst::IntRange heightRange = getRange(s->value("height"));

        if (widthRange.end <= 0 || heightRange.end <= 0)
        {
            continue;
        }

        // Iterate from min to max (160x120,320x240,640x480)
        //
        auto width = widthRange.start;
        auto height = heightRange.start;
        while (width <= widthRange.end && height <= heightRange.end)
        {
            sizes.append(QSize(width, height));
            width <<= 1;
            height <<= 1;
        }

        if ((width >> 1) != widthRange.end || (height >> 1) != heightRange.end)
        {
            // Iterate from max to min  (720x576,360x288,180x144)
            //
            width = widthRange.end;
            height = heightRange.end;
            while (width > widthRange.start && height > heightRange.start)
            {
                sizes.append(QSize(width, height));
                width >>= 1;
                height >>= 1;
            }
        }
    }

    // Sort resolutions descending
    //
    qSort(sizes.begin(), sizes.end(),
        [](const QSize &s1, const QSize &s2) { return s1.width() * s1.height() > s2.width() * s2.height(); }
        );

    // And remove duplicates
    //
    sizes.erase(std::unique (sizes.begin(), sizes.end()), sizes.end());

    Q_FOREACH(auto size, sizes)
    {
        QString name = tr("%1x%2").arg(size.width()).arg(size.height());
        listSizes->addItem(name, size);
        if (size == selectedSize)
        {
            listSizes->setCurrentIndex(listSizes->count() - 1);
        }
    }
}

void VideoSettings::videoCodecChanged(int index)
{
    auto on = !listVideoCodecs->itemData(index).isNull();

    if (spinFps)
    {
        spinFps->setEnabled(on);
    }
    spinBitrate->setEnabled(on);
    listVideoMuxers->setEnabled(on);
    checkDeinterlace->setEnabled(on);
}

// Return nullptr for 'default', otherwise the text itself
//
static QString getListText(const QComboBox* cb)
{
    auto idx = cb->currentIndex();
    return cb->itemData(idx).isNull()? nullptr: cb->itemText(idx);
}

static QVariant getListData(const QComboBox* cb)
{
    auto idx = cb->currentIndex();
    return cb->itemData(idx);
}

void VideoSettings::save()
{
    QSettings settings;
    auto device = getListData(listDevices).toStringList();
    if (device.isEmpty())
    {
        settings.remove("device-type");
        settings.remove("device");
    }
    else
    {
        settings.setValue("device-type", device.takeFirst());
        settings.setValue("device", device.takeFirst());
    }
    settings.setValue("video-channel", getListText(listChannels));
    settings.setValue("format",        getListData(listFormats));
    settings.setValue("size",          getListData(listSizes));
    settings.setValue("video-encoder", getListData(listVideoCodecs));
    settings.setValue("video-muxer",   getListData(listVideoMuxers));
    settings.setValue("rtp-payloader", getListData(listRtpPayloaders));
    settings.setValue("image-encoder", getListData(listImageCodecs));
    settings.setValue("enable-rtp",    checkEnableRtp->isChecked());
    settings.setValue("rtp-clients",   textRtpClients->text());
    settings.setValue("bitrate",       spinBitrate->value());
    settings.setValue("video-deinterlace", checkDeinterlace->isChecked());
    if (spinFps)
    {
        settings.setValue("video-max-fps", spinFps->value() > 0? spinFps->value(): QVariant());
    }
}
