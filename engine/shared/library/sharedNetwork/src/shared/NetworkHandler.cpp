// NetworkHandler.cpp
// Copyright 2000-02, Sony Online Entertainment Inc., all rights reserved.

//-----------------------------------------------------------------------

#include "sharedNetwork/FirstSharedNetwork.h"
#include "sharedNetwork/NetworkHandler.h"

#include "Archive/Archive.h"
#include "sharedDebug/DebugFlags.h"
#include "sharedDebug/PixCounter.h"
#include "sharedFoundation/Clock.h"
#include "sharedFoundation/ConfigFile.h"
#include "sharedFoundation/Watcher.h"
#include "sharedLog/Log.h"
#include "sharedLog/NetLogConnection.h"
#include "sharedNetwork/Address.h"
#include "sharedNetwork/ConfigSharedNetwork.h"
#include "sharedNetwork/Connection.h"
#include "sharedNetwork/ManagerHandler.h"
#include "sharedNetwork/Service.h"
#include "sharedNetwork/UdpLibraryMT.h"
#include "sharedSynchronization/Guard.h"

#include "TcpClient.h"

#include <algorithm>
#include <deque>
#include <map>
#include <set>
#include <vector>
#include <limits>
#include <cstddef>

//-----------------------------------------------------------------------

namespace
{
	template <typename T>
	inline T clampValue(T value, T lower, T upper)
	{
		if (value < lower)
			return lower;
		if (value > upper)
			return upper;
		return value;
	}
}

//-----------------------------------------------------------------------

namespace NetworkHandlerNamespace
{
	class AdaptiveDispatchController
	{
	public:
		AdaptiveDispatchController();

		unsigned int computeTimeBudget(unsigned int baseBudget, unsigned int baseQueueThreshold, std::size_t currentQueueSize);
		unsigned int computeQueueThreshold(unsigned int baseQueueThreshold, std::size_t currentQueueSize);
		void recordCycle(std::size_t currentQueueSize, std::size_t processedMessages, unsigned int elapsedMilliseconds);

	private:
		void ensureConfig();
		float calculateLoadRatio(unsigned int baseQueueThreshold, std::size_t currentQueueSize) const;

	private:
		float m_queueDepthEma;
		float m_processedEma;
		float m_timeEma;
		float m_smoothingFactor;
		float m_highWatermarkMultiplier;
		float m_lowWatermarkMultiplier;
		unsigned int m_minTimeMs;
		unsigned int m_maxTimeMs;
	};

	bool   ms_logThrottle = false;

#if PRODUCTION == 0
	PixCounter::ResetInteger s_bytesSentThisFrameForPix;
	PixCounter::ResetInteger s_bytesReceivedThisFrameForPix;
#endif
}

using namespace NetworkHandlerNamespace;

//-----------------------------------------------------------------------

namespace NetworkHandlerNamespace
{

AdaptiveDispatchController::AdaptiveDispatchController()
: m_queueDepthEma(0.0f),
	m_processedEma(0.0f),
	m_timeEma(0.0f),
	m_smoothingFactor(0.2f),
	m_highWatermarkMultiplier(1.5f),
	m_lowWatermarkMultiplier(0.5f),
	m_minTimeMs(25),
	m_maxTimeMs(250)
{
}

//-----------------------------------------------------------------------

void AdaptiveDispatchController::ensureConfig()
{
	m_smoothingFactor = clampValue(ConfigSharedNetwork::getAdaptiveDispatchSmoothingFactor(), 0.01f, 1.0f);
	m_highWatermarkMultiplier = ConfigSharedNetwork::getAdaptiveDispatchHighWatermarkMultiplier();
	m_lowWatermarkMultiplier = ConfigSharedNetwork::getAdaptiveDispatchLowWatermarkMultiplier();
	if (m_highWatermarkMultiplier <= m_lowWatermarkMultiplier)
		m_highWatermarkMultiplier = m_lowWatermarkMultiplier + 0.1f;
	m_minTimeMs = static_cast<unsigned int>(std::max(1, ConfigSharedNetwork::getAdaptiveDispatchMinTimeMilliseconds()));
	m_maxTimeMs = static_cast<unsigned int>(std::max(static_cast<int>(m_minTimeMs), ConfigSharedNetwork::getAdaptiveDispatchMaxTimeMilliseconds()));
}

//-----------------------------------------------------------------------

float AdaptiveDispatchController::calculateLoadRatio(unsigned int baseQueueThreshold, std::size_t currentQueueSize) const
{
	float const baseline = static_cast<float>(baseQueueThreshold > 0 ? baseQueueThreshold : 1u);
	float ratio = static_cast<float>(currentQueueSize) / baseline;
	if (m_queueDepthEma > 0.0f)
		ratio = std::max(ratio, m_queueDepthEma / baseline);
	return ratio;
}

//-----------------------------------------------------------------------

unsigned int AdaptiveDispatchController::computeTimeBudget(unsigned int baseBudget, unsigned int baseQueueThreshold, std::size_t currentQueueSize)
{
	ensureConfig();

	unsigned int const safeBase = baseBudget ? baseBudget : m_minTimeMs;
	unsigned int const baseline = clampValue(safeBase, m_minTimeMs, std::max(m_minTimeMs, m_maxTimeMs));

	float loadRatio = calculateLoadRatio(baseQueueThreshold, currentQueueSize);
	float const highWatermark = std::max(m_highWatermarkMultiplier, m_lowWatermarkMultiplier + 0.1f);

	float normalized = 0.0f;
	if (loadRatio <= m_lowWatermarkMultiplier)
		normalized = 0.0f;
	else if (loadRatio >= highWatermark)
		normalized = 1.0f;
	else
		normalized = (loadRatio - m_lowWatermarkMultiplier) / (highWatermark - m_lowWatermarkMultiplier);

	normalized = clampValue(normalized, 0.0f, 1.0f);

	if (m_processedEma > 0.0f && baseQueueThreshold > 0)
	{
		float const processedRatio = m_processedEma / static_cast<float>(baseQueueThreshold);
		if (processedRatio < 0.75f)
		{
			float const adjustment = clampValue((0.75f - processedRatio) / 0.75f, 0.0f, 1.0f);
			normalized = clampValue(normalized + adjustment * 0.5f, 0.0f, 1.0f);
		}
	}

	if (m_timeEma > 0.0f && m_timeEma < static_cast<float>(baseline))
	{
		float const idleRatio = 1.0f - clampValue(m_timeEma / static_cast<float>(baseline), 0.0f, 1.0f);
		normalized = clampValue(normalized * (1.0f - (idleRatio * 0.5f)), 0.0f, 1.0f);
	}

	unsigned int minBudget = static_cast<unsigned int>(static_cast<float>(baseline) * 0.75f);
	minBudget = clampValue(minBudget, m_minTimeMs, baseline);

	unsigned int maxBudget = std::max(baseline, m_maxTimeMs);
	if (maxBudget < minBudget)
		maxBudget = minBudget;

	unsigned int const result = static_cast<unsigned int>(static_cast<float>(minBudget) + static_cast<float>(maxBudget - minBudget) * normalized);
	return clampValue(result, m_minTimeMs, maxBudget);
}

//-----------------------------------------------------------------------

unsigned int AdaptiveDispatchController::computeQueueThreshold(unsigned int baseQueueThreshold, std::size_t currentQueueSize)
{
	ensureConfig();

	if (baseQueueThreshold == 0)
		return currentQueueSize > 0 ? static_cast<unsigned int>(currentQueueSize) : 1u;

	float loadRatio = calculateLoadRatio(baseQueueThreshold, currentQueueSize);
	float const highWatermark = std::max(m_highWatermarkMultiplier, m_lowWatermarkMultiplier + 0.1f);

	float normalized = 0.0f;
	if (loadRatio <= m_lowWatermarkMultiplier)
		normalized = 0.0f;
	else if (loadRatio >= highWatermark)
		normalized = 1.0f;
	else
		normalized = (loadRatio - m_lowWatermarkMultiplier) / (highWatermark - m_lowWatermarkMultiplier);

	normalized = clampValue(normalized, 0.0f, 1.0f);

	float const minFactor = clampValue(m_lowWatermarkMultiplier, 0.1f, 1.0f);
	float const factor = 1.0f - normalized * (1.0f - minFactor);

	unsigned int threshold = static_cast<unsigned int>(static_cast<float>(baseQueueThreshold) * factor);
	if (threshold == 0)
		threshold = 1;
	return threshold;
}

//-----------------------------------------------------------------------

void AdaptiveDispatchController::recordCycle(std::size_t currentQueueSize, std::size_t processedMessages, unsigned int elapsedMilliseconds)
{
	ensureConfig();

	float const smoothing = clampValue(m_smoothingFactor, 0.01f, 1.0f);

	float const queueSample = static_cast<float>(currentQueueSize);
	float const processedSample = static_cast<float>(processedMessages);
	float const timeSample = static_cast<float>(elapsedMilliseconds);

	if (m_queueDepthEma <= 0.0f)
		m_queueDepthEma = queueSample;
	else
		m_queueDepthEma = (smoothing * queueSample) + ((1.0f - smoothing) * m_queueDepthEma);

	if (m_processedEma <= 0.0f)
		m_processedEma = processedSample;
	else
		m_processedEma = (smoothing * processedSample) + ((1.0f - smoothing) * m_processedEma);

	if (m_timeEma <= 0.0f)
		m_timeEma = timeSample;
	else
		m_timeEma = (smoothing * timeSample) + ((1.0f - smoothing) * m_timeEma);
}

} // namespace NetworkHandlerNamespace

//-----------------------------------------------------------------------

struct IncomingData
{
	Watcher<Connection> connection;
	Archive::ByteStream byteStream;
};

struct Services
{
	Services();
	~Services();
	std::set<UdpManagerMT *>      udpServices;
	std::vector<UdpManagerMT *>   deferredDestroyUdpManagers;
	std::deque<IncomingData>      inputQueue;
	std::vector<Service *>        services;
};

static Services services;
int s_bytesSentThisFrame = 0;
int s_bytesReceivedThisFrame = 0;
int s_packetsSentThisFrame = 0;
int s_packetsReceivedThisFrame = 0;
std::map<std::string, std::pair<int, int> > s_messageCount;
bool s_updating = false;
bool s_logBackloggedPackets = true;
bool s_removing = false;
static std::vector<Connection *>     deferredDisconnects;
AdaptiveDispatchController s_adaptiveDispatchController;
uint64 s_currentFrame = 0;

//-----------------------------------------------------------------------

Services::Services()
{
	UdpLibraryMT::getMutex();
}

//-----------------------------------------------------------------------

Services::~Services()
{
	std::set<UdpManagerMT *>::const_iterator i;
	for(i = udpServices.begin(); i != udpServices.end(); ++i)
	{
		(*i)->Release();
	}
	udpServices.clear();
}

//-----------------------------------------------------------------------

NetworkHandler::NetworkHandler() :
m_bindPort(0),
m_bindAddress(""),
m_udpManager(0),
m_managerHandler(0)
{
	m_managerHandler = new ManagerHandler(this);
}

//-----------------------------------------------------------------------

NetworkHandler::~NetworkHandler()
{
	if (m_udpManager)
		m_udpManager->ClearHandler();

	m_managerHandler->setOwner(0);
	m_managerHandler->Release();
	m_managerHandler = 0;

	if (m_udpManager)
	{
		if (s_updating)
			services.deferredDestroyUdpManagers.push_back(m_udpManager);
		else
		{
			std::set<UdpManagerMT *>::iterator f = services.udpServices.find(m_udpManager);
			if (f != services.udpServices.end())
				services.udpServices.erase(f);
			m_udpManager->Release();
			m_udpManager = 0;
		}
	}
}

//-----------------------------------------------------------------------

void NetworkHandler::dispatch()
{
	LogManager::update();

	UdpLibraryMT::mainThreadUpdate();

	if (!services.inputQueue.empty())
	{
		size_t const startQueueSize = services.inputQueue.size();
		unsigned long const startTime = Clock::timeMs();
		bool throttle = ConfigSharedNetwork::getNetworkHandlerDispatchThrottle();
		unsigned int const baseProcessTimeMilliseconds = static_cast<unsigned int>(ConfigSharedNetwork::getNetworkHandlerDispatchThrottleTimeMilliseconds());
		unsigned int const baseQueueSize = static_cast<unsigned int>(ConfigSharedNetwork::getNetworkHandlerDispatchQueueSize());
		bool const adaptiveDispatchEnabled = ConfigSharedNetwork::getUseAdaptiveDispatch();

		unsigned int processTimeMilliseconds = baseProcessTimeMilliseconds;
		unsigned int queueSize = baseQueueSize;

		if (!throttle)
			processTimeMilliseconds = std::numeric_limits<unsigned int>::max();

		if (adaptiveDispatchEnabled)
		{
			throttle = true;
			processTimeMilliseconds = s_adaptiveDispatchController.computeTimeBudget(baseProcessTimeMilliseconds, baseQueueSize, services.inputQueue.size());
			queueSize = s_adaptiveDispatchController.computeQueueThreshold(baseQueueSize, services.inputQueue.size());
		}

		size_t messagesProcessed = 0;

		while (!services.inputQueue.empty() && (services.inputQueue.size() > queueSize || (!throttle || (throttle && ((Clock::timeMs() - startTime) < processTimeMilliseconds)))))
		{
			std::deque<IncomingData>::iterator i = services.inputQueue.begin();

			Connection * c = (*i).connection;
			if (c)
			{
				try
				{
					reportBytesReceived((*i).byteStream.getSize());
					int sendSize = (*i).byteStream.getSize();
					static const int packetSizeWarnThreshold = ConfigSharedNetwork::getPacketSizeWarnThreshold();
					if(packetSizeWarnThreshold > 0)
					{
						WARNING(sendSize > packetSizeWarnThreshold, ("large packet received (%d bytes) exceeds warning threshold %d defined as SharedNetwork/packetSizeWarnThreshold", sendSize, packetSizeWarnThreshold));
					}

					c->receive((*i).byteStream);
				}
				catch(const Archive::ReadException & readException)
				{
					WARNING(true, ("Unhandled Archive read error (%s) on connection. Continuing to throw from NetwokrHandler::Dispatch", readException.what()));
					throw(readException);
				}
			}

			services.inputQueue.pop_front();
			++messagesProcessed;
		}

		unsigned long const elapsedTimeMs = Clock::timeMs() - startTime;
		if (adaptiveDispatchEnabled)
		{
			unsigned int const elapsedForRecord = elapsedTimeMs > static_cast<unsigned long>(std::numeric_limits<unsigned int>::max()) ? std::numeric_limits<unsigned int>::max() : static_cast<unsigned int>(elapsedTimeMs);
			s_adaptiveDispatchController.recordCycle(services.inputQueue.size(), messagesProcessed, elapsedForRecord);
		}

		REPORT_LOG(ms_logThrottle && throttle && !services.inputQueue.empty(), ("NetworkHandler::dispatch: services.inputQueue has %d/%d messages remaining\n", services.inputQueue.size(), startQueueSize));
	}
}

//-----------------------------------------------------------------------

void NetworkHandler::flushAndConfirmAll()
{
	static bool enabled = ConfigSharedNetwork::getEnableFlushAndConfirmAllData();

	if(enabled)
	{
		std::vector<Service *>::iterator i;
		for(i = services.services.begin(); i != services.services.end(); ++i)
		{
			(*i)->flushAndConfirmAllData();
		}
	}
	else
	{
		update();
	}
}

//-----------------------------------------------------------------------

const std::string & NetworkHandler::getBindAddress() const
{
	return m_bindAddress;
}

//-----------------------------------------------------------------------

const unsigned short NetworkHandler::getBindPort() const
{
	return m_bindPort;
}

//-----------------------------------------------------------------------

void NetworkHandler::newManager(UdpManagerMT * u)
{
	services.udpServices.insert(u);
}

//-----------------------------------------------------------------------

void NetworkHandler::newService(Service * s)
{
	services.services.push_back(s);
}

//-----------------------------------------------------------------------

void NetworkHandler::removeService(Service * s)
{
	std::vector<Service *>::iterator f = std::find(services.services.begin(), services.services.end(), s);
	if(f != services.services.end())
		services.services.erase(f);
}

//-----------------------------------------------------------------------

void NetworkHandler::onConnect(void * callback, UdpConnectionMT * connection)
{
	if(callback)
	{
		NetworkHandler * h = reinterpret_cast<NetworkHandler *>(callback);
		if(connection)
		{
			h->onConnectionOpened(connection);
		}
	}
}

//-----------------------------------------------------------------------

void NetworkHandler::onReceive(Connection * c, const unsigned char * d, int s)
{
	if(c)
	{
		services.inputQueue.emplace_back(IncomingData());
		services.inputQueue.back().connection = c;
		services.inputQueue.back().byteStream.put(d, s);

		static const bool logAllNetworkTraffic = ConfigSharedNetwork::getLogAllNetworkTraffic();

		if(logAllNetworkTraffic)
		{
			if(! dynamic_cast<const NetLogConnection *>(c))
			{
				std::string logChan = "Network:Recv:" + c->getRemoteAddress() + ":";
				char portBuf[7] = {"\0"};
				snprintf(portBuf, sizeof(portBuf), "%d", c->getRemotePort());
				logChan += portBuf;
				std::string output;
				const unsigned char * uc = d;
				while(uc < d + s)
				{
					if(isalpha(*uc))
					{
						char s = *uc;
						output += s;
					}
					else
					{
						char numBuf[128] = {"\0"};
						snprintf(numBuf, sizeof(numBuf), "[%d]", *uc);
						output += numBuf;
					}
					++uc;
				}
				LOG(logChan, ("%s", output.c_str()));
			}
		}
	}
}

//-----------------------------------------------------------------------

void NetworkHandler::onReceive(void *, UdpConnectionMT * u, const unsigned char * d, int s)
{
	Connection * c = reinterpret_cast<Connection *>(u->GetPassThroughData());
	onReceive(c, d, s);
}

//-----------------------------------------------------------------------

void NetworkHandler::onTerminate(Connection * c)
{
	if (c)
	{
		c->setDisconnectReason("NetworkHandler::onTerminate called");
		c->disconnect();
	}
}

//-----------------------------------------------------------------------

void NetworkHandler::onTerminate(void * m, UdpConnectionMT * u)
{
	if(m)
	{
		if(u)
		{
			u->AddRef();
			Connection * c = reinterpret_cast<Connection *>(u->GetPassThroughData());
			onTerminate(c);
			u->SetPassThroughData(0);
			u->Release();
		}
	}
}

//-----------------------------------------------------------------------

void NetworkHandler::setBindAddress(const std::string & address)
{
	m_bindAddress = address;
}

//-----------------------------------------------------------------------

void NetworkHandler::setBindPort(const unsigned short p)
{
	m_bindPort = p;
}

//-----------------------------------------------------------------------

void NetworkHandler::install()
{
	Connection::install();
	UdpLibraryMT::install();

	DebugFlags::registerFlag(ms_logThrottle, "SharedNetwork", "networkHandlerDispatchLogThrottle");
	DebugFlags::registerFlag(s_logBackloggedPackets, "SharedNetwork", "logBackloggedPackets");

#if PRODUCTION == 0
	s_bytesSentThisFrameForPix.bindToCounter("NetworkBytesSent");
	s_bytesReceivedThisFrameForPix.bindToCounter("NetworkBytesReceived");
#endif
}

//-----------------------------------------------------------------------

void NetworkHandler::remove()
{
	s_removing = true;
	while (!services.services.empty())
	{
		// Service destructor calls removeService, which should the service at
		// begin() from the list
		delete *services.services.begin();
	}

	UdpLibraryMT::remove();
	Connection::remove();

	DebugFlags::unregisterFlag(ms_logThrottle);
	DebugFlags::unregisterFlag(s_logBackloggedPackets);
}

//-----------------------------------------------------------------------

void NetworkHandler::update()
{
	if (!UdpLibraryMT::getUseNetworkThread())
	{
		std::set<UdpManagerMT *>::const_iterator i;
		std::set<UdpManagerMT *>::const_iterator begin = services.udpServices.begin();
		std::set<UdpManagerMT *>::const_iterator end = services.udpServices.end();

		s_updating = true;
		for(i = begin; i != end; ++i)
			(*i)->GiveTime();
		std::vector<Service *>::const_iterator is;
		for(is = services.services.begin(); is != services.services.end(); ++is)
		{
			(*is)->updateTcp();
		}

		Connection::update();
		TcpClient::flushPendingWrites();

		s_updating = false;

		std::vector<UdpManagerMT *>::const_iterator im;
		for(im = services.deferredDestroyUdpManagers.begin(); im != services.deferredDestroyUdpManagers.end(); ++im)
		{
			UdpManagerMT * u = (*im);
			std::set<UdpManagerMT *>::iterator f = services.udpServices.find(u);
			if(f != services.udpServices.end())
				services.udpServices.erase(f);
			if(u)
				u->Release();
		}
		services.deferredDestroyUdpManagers.clear();
	}

	Guard lock(UdpLibraryMT::getMutex());
	while (!deferredDisconnects.empty())
	{
		Connection *c = *deferredDisconnects.begin();
		deferredDisconnects.erase(deferredDisconnects.begin());
		c->onConnectionClosed();
		delete c;
	}
}

//-----------------------------------------------------------------------

const int NetworkHandler::getBytesReceivedThisFrame()
{
	return s_bytesReceivedThisFrame;
}

//-----------------------------------------------------------------------

const int NetworkHandler::getBytesSentThisFrame()
{
	return s_bytesSentThisFrame;
}

//-----------------------------------------------------------------------

void NetworkHandler::reportBytesReceived(const int bytes)
{
	s_bytesReceivedThisFrame+=bytes;
	s_packetsReceivedThisFrame++;
#if PRODUCTION == 0
	s_bytesReceivedThisFrameForPix += bytes;
#endif
}

//-----------------------------------------------------------------------

void NetworkHandler::reportBytesSent(const int bytes)
{
	s_bytesSentThisFrame+=bytes;
	s_packetsSentThisFrame++;
#if PRODUCTION == 0
	s_bytesSentThisFrameForPix += bytes;
#endif
}

//-----------------------------------------------------------------------

const int NetworkHandler::getPacketsReceivedThisFrame()
{
	return s_packetsReceivedThisFrame;
}

//-----------------------------------------------------------------------

const int NetworkHandler::getPacketsSentThisFrame()
{
	return s_packetsSentThisFrame;
}

//-----------------------------------------------------------------------

void NetworkHandler::reportMessage(const std::string & messageName, const int size)
{
	static const bool reportMessages = ConfigSharedNetwork::getReportMessages();
	if(reportMessages)
	{
		std::map<std::string, std::pair<int, int> >::iterator f = s_messageCount.find(messageName);
		if(f != s_messageCount.end())
		{
			(*f).second.first++;
			(*f).second.second+=size;
		}
		else
		{
			std::pair<int, int> newEntry = std::make_pair(1, size);
			s_messageCount[messageName] = newEntry;
		}
	}
}

//-----------------------------------------------------------------------

void NetworkHandler::clearBytesThisFrame()
{
	s_currentFrame++;
	Guard lock(UdpLibraryMT::getMutex());

	bool logMessageCounts = false;
	static const int backlogThreshold = ConfigSharedNetwork::getLogBackloggedPacketThreshold();
	if(s_logBackloggedPackets && backlogThreshold > 0)
	{
		std::vector<Service *>::iterator i;
		for(i = services.services.begin(); i != services.services.end(); ++i)
		{
			(*i)->logBackloggedPackets();
		}
	}

	static const int packetCountWarnThreshold = ConfigSharedNetwork::getPacketCountWarnThreshold();
	if(packetCountWarnThreshold > 0)
	{
		if(s_packetsSentThisFrame > packetCountWarnThreshold)
		{
			LOG("Network:ExcessivePackets:Send", ("packet send count (%d packets) for this frame exceeds warning threshold %d defined as SharedNetwork/packetCountWarnThreshold", NetworkHandler::getPacketsSentThisFrame(), packetCountWarnThreshold));
			logMessageCounts = true;
		}
		if(s_packetsReceivedThisFrame > packetCountWarnThreshold)
		{
			LOG("Network:ExcessivePackets:Recv", ("packet receive count (%d packets) for this frame exceeds warning threshold %d defined as SharedNetwork/packetCountWarnThreshold", s_packetsReceivedThisFrame, packetCountWarnThreshold));
			logMessageCounts = true;
		}
	}

	static const int byteCountWarnThreshold = ConfigSharedNetwork::getByteCountWarnThreshold();
	if(byteCountWarnThreshold > 0)
	{
		if(s_bytesSentThisFrame > byteCountWarnThreshold)
		{
			LOG("Network:ExcessiveBytes:Send", ("byte send count (%d bytes) for this frame exceeds warning threshold %d defined as SharedNetwork/byteCountWarnThreshold", s_bytesSentThisFrame, byteCountWarnThreshold));
			logMessageCounts = true;
		}
		if(s_bytesReceivedThisFrame > byteCountWarnThreshold)
		{
			LOG("Network:ExcessiveBytes:Recv", ("byte receive count (%d bytes) for this frame exceeds warning threshold %d defined as SharedNetwork/byteCountWarnThreshold", s_bytesReceivedThisFrame, byteCountWarnThreshold));
			logMessageCounts = true;
		}
	}


	if(logMessageCounts)
	{
		static const bool reportMessages = ConfigSharedNetwork::getReportMessages();
		if(reportMessages)
		{
			std::map<std::string, std::pair<int, int> >::iterator i;
			static char msgBuf[256];
			for(i = s_messageCount.begin(); i != s_messageCount.end(); ++i)
			{
				snprintf(msgBuf, sizeof(msgBuf), "[%s] - %d messages, %d total bytes", (*i).first.c_str(), (*i).second.first, (*i).second.second);
				LOG("Network:MessageCount", ("%s", msgBuf));
			}
		}
	}

	s_messageCount.clear();
	s_bytesReceivedThisFrame = 0;
	s_bytesSentThisFrame = 0;
	s_packetsReceivedThisFrame = 0;
	s_packetsSentThisFrame = 0;

}

//-----------------------------------------------------------------------

LogicalPacket const *NetworkHandler::createPacket(unsigned char const *data, int size)
{
	if (m_udpManager)
		return m_udpManager->CreatePacket(data, size);
	return UdpMiscMT::CreateQuickLogicalPacket(data, size);
}

//-----------------------------------------------------------------------

void NetworkHandler::releasePacket(LogicalPacket const *p)
{
	p->Release();
}

//-----------------------------------------------------------------------

bool NetworkHandler::removing()
{
	return s_removing;
}

//-----------------------------------------------------------------------

bool NetworkHandler::isPortReserved(unsigned short p)
{
	return(ConfigSharedNetwork::getIsPortReserved(p));
}

//-----------------------------------------------------------------------

void NetworkHandler::disconnectConnection(Connection * c)
{
	if(! s_removing)
	{
		std::vector<Connection *>::iterator f = std::find(deferredDisconnects.begin(), deferredDisconnects.end(), c);
		if(f == deferredDisconnects.end())
		{
			deferredDisconnects.push_back(c);
		}
	}
}

//-----------------------------------------------------------------------

uint64 NetworkHandler::getCurrentFrame()
{
	return s_currentFrame;
}

//-----------------------------------------------------------------------

int NetworkHandler::getRecvTotalCompressedByteCount()
{
	return ManagerHandler::getRecvTotalCompressedByteCount();
}

//-----------------------------------------------------------------------

int NetworkHandler::getRecvTotalUncompressedByteCount()
{
	return ManagerHandler::getRecvTotalUncompressedByteCount();
}

//-----------------------------------------------------------------------

int NetworkHandler::getSendTotalCompressedByteCount()
{
	return ManagerHandler::getSendTotalCompressedByteCount();
}

//-----------------------------------------------------------------------

int NetworkHandler::getSendTotalUncompressedByteCount()
{
	return ManagerHandler::getSendTotalUncompressedByteCount();
}

//-----------------------------------------------------------------------

float NetworkHandler::getTotalCompressionRatio()
{
	return ManagerHandler::getTotalCompressionRatio();
}

//-----------------------------------------------------------------------

bool NetworkHandler::isAddressLocal(const std::string & address)
{
	Address addr(address, 0);

	bool result = false;
	const std::vector<std::pair<std::string, std::string> > & localAddresses = NetworkHandler::getInterfaceAddresses();
	std::vector<std::pair<std::string, std::string> >::const_iterator i;
	for(i = localAddresses.begin(); i != localAddresses.end(); ++i)
	{
		if(i->second == addr.getHostAddress())
		{
			result = true;
			break;
		}
	}
	return result;
}

//-----------------------------------------------------------------------

