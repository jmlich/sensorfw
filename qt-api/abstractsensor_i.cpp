/**
   @file abstractsensor_i.cpp
   @brief Base class for sensor interface

   <p>
   Copyright (C) 2009-2010 Nokia Corporation

   @author Joep van Gassel <joep.van.gassel@nokia.com>
   @author Timo Rongas <ext-timo.2.rongas@nokia.com>
   @author Antti Virtanen <antti.i.virtanen@nokia.com>
   @author Lihan Guo <ext-lihan.4.guo@nokia.com>

   This file is part of Sensord.

   Sensord is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License
   version 2.1 as published by the Free Software Foundation.

   Sensord is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with Sensord.  If not, see <http://www.gnu.org/licenses/>.
   </p>
 */

#include "sensormanagerinterface.h"
#include "abstractsensor_i.h"

struct AbstractSensorChannelInterface::AbstractSensorChannelInterfaceImpl
{
    AbstractSensorChannelInterfaceImpl(QObject* parent, int sessionId);

    SensorError errorCode_;
    QString errorString_;
    int sessionId_;
    int interval_;
    unsigned int bufferInterval_;
    unsigned int bufferSize_;
    SocketReader socketReader_;
    bool running_;
    bool standbyOverride_;
};

AbstractSensorChannelInterface::AbstractSensorChannelInterfaceImpl::AbstractSensorChannelInterfaceImpl(QObject* parent, int sessionId) :
    errorCode_(SNoError),
    errorString_(""),
    sessionId_(sessionId),
    interval_(0),
    bufferInterval_(0),
    bufferSize_(1),
    socketReader_(parent),
    running_(false),
    standbyOverride_(false)
{
}

AbstractSensorChannelInterface::AbstractSensorChannelInterface(const QString& path, const char* interfaceName, int sessionId) :
    QDBusAbstractInterface(SERVICE_NAME, path, interfaceName, QDBusConnection::systemBus(), 0),
    pimpl_(new AbstractSensorChannelInterfaceImpl(this, sessionId))
{
    if (!pimpl_->socketReader_.initiateConnection(sessionId)) {
        setError(SClientSocketError, "Socket connection failed.");
    }
}

AbstractSensorChannelInterface::~AbstractSensorChannelInterface()
{
    if ( isValid() )
        release();
    if (!pimpl_->socketReader_.dropConnection())
        setError(SClientSocketError, "Socket disconnect failed.");
    delete pimpl_;
}

SocketReader& AbstractSensorChannelInterface::getSocketReader() const
{
    return pimpl_->socketReader_;
}

bool AbstractSensorChannelInterface::release()
{
    // ToDo: note that after release this interace becomes invalid (this should be handled correctly)
    return SensorManagerInterface::instance().releaseInterface(id(), pimpl_->sessionId_);
}

void AbstractSensorChannelInterface::setError(SensorError errorCode, const QString& errorString)
{
    pimpl_->errorCode_   = errorCode;
    pimpl_->errorString_ = errorString;

    //emit errorSignal(errorCode);
}

QDBusReply<void> AbstractSensorChannelInterface::start()
{
    return start(pimpl_->sessionId_);
}

QDBusReply<void> AbstractSensorChannelInterface::stop()
{
    return stop(pimpl_->sessionId_);
}

QDBusReply<void> AbstractSensorChannelInterface::start(int sessionId)
{
    clearError();

    if (pimpl_->running_) {
        return QDBusReply<void>();
    }
    pimpl_->running_ = true;

    connect(pimpl_->socketReader_.socket(), SIGNAL(readyRead()), this, SLOT(dataReceived()));

    QList<QVariant> argumentList;
    argumentList << qVariantFromValue(sessionId);
    QDBusReply<void> returnValue = callWithArgumentList(QDBus::Block, QLatin1String("start"), argumentList);

    if (pimpl_->standbyOverride_)
    {
        setStandbyOverride(sessionId, true);
    }
    /// Send interval request when started.
    setInterval(sessionId, pimpl_->interval_);
    setBufferInterval(sessionId, pimpl_->bufferInterval_);
    setBufferSize(sessionId, pimpl_->bufferSize_);

    return returnValue;
}

QDBusReply<void> AbstractSensorChannelInterface::stop(int sessionId)
{
    clearError();

    if (!pimpl_->running_) {
        return QDBusReply<void>();
    }
    pimpl_->running_ = false ;

    disconnect(pimpl_->socketReader_.socket(), SIGNAL(readyRead()), this, SLOT(dataReceived()));
    setStandbyOverride(sessionId, false);
    /// Drop interval requests when stopped
    setInterval(sessionId, 0);

    QList<QVariant> argumentList;
    argumentList << qVariantFromValue(sessionId);
    return callWithArgumentList(QDBus::Block, QLatin1String("stop"), argumentList);
}

QDBusReply<void> AbstractSensorChannelInterface::setInterval(int sessionId, int value)
{
    clearError();

    QList<QVariant> argumentList;
    argumentList << qVariantFromValue(sessionId) << qVariantFromValue(value);
    return callWithArgumentList(QDBus::Block, QLatin1String("setInterval"), argumentList);
}

QDBusReply<void> AbstractSensorChannelInterface::setBufferInterval(int sessionId, unsigned int value)
{
    clearError();

    QList<QVariant> argumentList;
    argumentList << qVariantFromValue(sessionId) << qVariantFromValue(value);
    return callWithArgumentList(QDBus::Block, QLatin1String("setBufferInterval"), argumentList);
}

QDBusReply<void> AbstractSensorChannelInterface::setBufferSize(int sessionId, unsigned int value)
{
    clearError();

    QList<QVariant> argumentList;
    argumentList << qVariantFromValue(sessionId) << qVariantFromValue(value);
    return callWithArgumentList(QDBus::Block, QLatin1String("setBufferSize"), argumentList);
}

QDBusReply<bool> AbstractSensorChannelInterface::setStandbyOverride(int sessionId, bool value)
{
    clearError();

    QList<QVariant> argumentList;
    argumentList << qVariantFromValue(sessionId) << qVariantFromValue(value);
    return callWithArgumentList(QDBus::Block, QLatin1String("setStandbyOverride"), argumentList);
}

DataRangeList AbstractSensorChannelInterface::getAvailableDataRanges()
{
    QDBusReply<DataRangeList> ret = call(QDBus::Block, QLatin1String("getAvailableDataRanges"));
    return ret.value();
}

DataRange AbstractSensorChannelInterface::getCurrentDataRange()
{
    clearError();
    QDBusReply<DataRange> retVal = call(QDBus::Block, QLatin1String("getCurrentDataRange"));
    return retVal.value();
}

void AbstractSensorChannelInterface::requestDataRange(DataRange range)
{
    clearError();
    call(QDBus::Block, QLatin1String("requestDataRange"), qVariantFromValue(pimpl_->sessionId_), qVariantFromValue(range));
}

void AbstractSensorChannelInterface::removeDataRangeRequest()
{
    clearError();
    call(QDBus::Block, QLatin1String("removeDataRangeRequest"), qVariantFromValue(pimpl_->sessionId_));
}

DataRangeList AbstractSensorChannelInterface::getAvailableIntervals()
{
    QDBusReply<DataRangeList> ret = call(QDBus::Block, QLatin1String("getAvailableIntervals"));
    return ret.value();
}

IntegerRangeList AbstractSensorChannelInterface::getAvailableBufferIntervals()
{
    QDBusReply<IntegerRangeList> ret = call(QDBus::Block, QLatin1String("getAvailableBufferIntervals"));
    return ret.value();
}

IntegerRangeList AbstractSensorChannelInterface::getAvailableBufferSizes()
{
    QDBusReply<IntegerRangeList> ret = call(QDBus::Block, QLatin1String("getAvailableBufferSizes"));
    return ret.value();
}

bool AbstractSensorChannelInterface::hwBuffering()
{
    QDBusReply<bool> ret = call(QDBus::Block, QLatin1String("hwBuffering"));
    return ret.value();
}

int AbstractSensorChannelInterface::sessionId() const
{
    return pimpl_->sessionId_;
}

SensorError AbstractSensorChannelInterface::errorCode() const
{
    // TODO: This solution may introduce problems, if errors are
    //       not cleared before another happens.
    if (pimpl_->errorCode_ != SNoError) {
        return pimpl_->errorCode_;
    }
    return static_cast<SensorError>(errorCodeInt());
}

QString AbstractSensorChannelInterface::errorString() const
{
    if (pimpl_->errorCode_ != SNoError) {
        return pimpl_->errorString_;
    }
    return qvariant_cast<QString>(internalPropGet("errorString"));
}

QString AbstractSensorChannelInterface::description() const
{
    return qvariant_cast<QString>(internalPropGet("description"));
}

QString AbstractSensorChannelInterface::id() const
{
    return qvariant_cast<QString>(internalPropGet("id"));
}

int AbstractSensorChannelInterface::interval() const
{
    if (pimpl_->running_)
        return qvariant_cast<int>(internalPropGet("interval"));
    return pimpl_->interval_;
}

void AbstractSensorChannelInterface::setInterval(int value)
{
    pimpl_->interval_ = value;
    if (pimpl_->running_)
        setInterval(pimpl_->sessionId_, value);
}

unsigned int AbstractSensorChannelInterface::bufferInterval() const
{
    if (pimpl_->running_)
        return qvariant_cast<unsigned int>(internalPropGet("bufferInterval"));
    return pimpl_->bufferInterval_;
}

void AbstractSensorChannelInterface::setBufferInterval(unsigned int value)
{
    pimpl_->bufferInterval_ = value;
    if (!pimpl_->running_)
        setBufferInterval(pimpl_->sessionId_, value);
}

unsigned int AbstractSensorChannelInterface::bufferSize() const
{
    if (pimpl_->running_)
        return qvariant_cast<unsigned int>(internalPropGet("bufferSize"));
    return pimpl_->bufferSize_;
}

void AbstractSensorChannelInterface::setBufferSize(unsigned int value)
{
    pimpl_->bufferSize_ = value;
    if (!pimpl_->running_)
        setBufferSize(pimpl_->sessionId_, value);
}

bool AbstractSensorChannelInterface::standbyOverride() const
{
    if (pimpl_->running_)
        return qvariant_cast<bool>(internalPropGet("standbyOverride"));
    return pimpl_->standbyOverride_;
}

bool AbstractSensorChannelInterface::setStandbyOverride(bool override)
{
    pimpl_->standbyOverride_ = override;
    return setStandbyOverride(pimpl_->sessionId_, override);
}

QString AbstractSensorChannelInterface::type() const
{
    return qvariant_cast<QString>(internalPropGet("type"));
}

int AbstractSensorChannelInterface::errorCodeInt() const
{
    return static_cast<SensorManagerError>(qvariant_cast<int>(internalPropGet("errorCodeInt")));
}

void AbstractSensorChannelInterface::clearError()
{
    pimpl_->errorCode_ = SNoError;
    pimpl_->errorString_.clear();
}

void AbstractSensorChannelInterface::dataReceived()
{
    do
    {
        if(!dataReceivedImpl())
            return;
    } while(pimpl_->socketReader_.socket()->bytesAvailable());
}

bool AbstractSensorChannelInterface::read(void* buffer, int size)
{
    return pimpl_->socketReader_.read(buffer, size);
}

bool AbstractSensorChannelInterface::setDataRangeIndex(int dataRangeIndex)
{
    clearError();
    call(QDBus::Block, QLatin1String("setDataRangeIndex"), qVariantFromValue(pimpl_->sessionId_), qVariantFromValue(dataRangeIndex));

    DataRangeList ranges = getAvailableDataRanges();
    DataRange range = getCurrentDataRange();
    return ranges.at(dataRangeIndex)==range;
}
