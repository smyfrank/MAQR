#include "maqr-routing-protocol.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/uinteger.h"
#include "ns3/wifi-net-device.h"
#include "ns3/adhoc-wifi-mac.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("MaqrRoutingProtocol");

namespace maqr {

NS_OBJECT_ENSURE_REGISTERED(RoutingProtocol);

const uint32_t RoutingProtocol::MAQR_PORT = 5678;

// Set metadata
TypeId RoutingProtocol::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::maqr::RoutingProtocol")
    .SetParent<Ipv4RoutingProtocol>()
    .SetGroupName("MAQR")
    .AddConstructor<RoutingProtocol>()
    // the access of internal member objects of a simulation
    .AddAttribute("HelloInterval", "Periodic interval for hello packet",
                  TimeValue(Seconds(1)),
                  MakeTimeAccessor(&RoutingProtocol::m_helloInterval), MakeTimeChecker())
    ;
}

RoutingProtocol::RoutingProtocol()
  : m_helloInterval(Seconds(1)),
    m_maxQueueLen(64),
    m_maxQueueTime(Seconds(30)),
    m_queue(m_maxQueueLen, m_maxQueueTime),
    m_helloIntervalTimer(Timer::CANCEL_ON_DESTROY),
    m_qLearning(0.3, 0.9, 0.3, Seconds(1.1))
{
  m_nb = Neighbors(Seconds(2));  // neighbor entry lifetime
  m_uniformRandomVariable = CreateObject<UniformRandomVariable> ();
}

RoutingProtocol::~RoutingProtocol()
{

}

void RoutingProtocol::DoDispose()
{
  m_ipv4 = 0;
  for(auto iter = m_socketAddresses.begin(); iter != m_socketAddresses.end(); ++iter)
  {
    iter->first->Close();
  }
  m_socketAddresses.clear();
  Ipv4RoutingProtocol::DoDispose();
}

Ptr<Ipv4Route> RoutingProtocol::RouteOutput(Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif,
                                            Socket::SocketErrno &socketerr)
{

}

bool RoutingProtocol::RouteInput(Ptr<const Packet> p, const Ipv4Header &header,
                                 Ptr<const NetDevice> idev, UnicastForwardCallback ucb,
                                 MulticastForwardCallback mcb, LocalDeliverCallback lcb,
                                 ErrorCallback ecb)
{
  
}

void RoutingProtocol::NotifyInterfaceUp(uint32_t interface)
{
  NS_LOG_FUNCTION(this << m_ipv4->GetAddress(interface, 0).GetLocal());
  Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol>();
  if(l3->GetNAddresses(interface) > 1)
  {
    NS_LOG_WARN("MAQR does not work with more than one address per each interface.");
  }
  Ipv4InterfaceAddress iface = l3->GetAddress(interface, 0);
  if(iface.GetLocal() == Ipv4Address("127.0.0.1"))
  {
    return;
  }

  // Create a socket to listen only on this interface
  Ptr<Socket> socket = Socket::CreateSocket(GetObject<Node>(), UdpSocketFactory::GetTypeId());
  NS_ASSERT(socket != 0);
  /**
   * \TODO: SetRecvCallback
   */
  socket->SetRecvCallback(MakeCallback(&RoutingProtocol::ReceiveHello, this));
  socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), MAQR_PORT));
  socket->BindToNetDevice(l3->GetNetDevice(interface));
  socket->SetAllowBroadcast(true);
  socket->SetAttribute("IpTtl", UintegerValue(1));
  m_socketAddresses.insert(std::make_pair(socket, iface));

  // Allow neighbor manager use this interface for layer 2 feedback if possible
  Ptr<NetDevice> dev = m_ipv4->GetNetDevice(m_ipv4->GetInterfaceForAddress(iface.GetLocal()));
  Ptr<WifiNetDevice> wifi = dev->GetObject<WifiNetDevice>();
  if(wifi == 0)
  {
    return;
  }
  Ptr<WifiMac> mac = wifi->GetMac();
  if(mac == 0)
  {
    return;
  }
  mac->TraceConnectWithoutContext("TxErrHeader", m_nb.GetTxErrorCallback());
}

void RoutingProtocol::NotifyInterfaceDown(uint32_t interface)
{
  NS_LOG_FUNCTION(this << m_ipv4->GetAddress(interface, 0).GetLocal());

  // Disable layer 2 link state monitoring (if possible) which registed in NotifyInterfaceUp
  Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol>();
  Ptr<NetDevice> dev = l3->GetNetDevice(interface);
  Ptr<WifiNetDevice> wifi = l3->GetObject<WifiNetDevice>();
  if(wifi != 0)
  {
    Ptr<WifiMac> mac = wifi->GetMac()->GetObject<AdhocWifiMac>();
    if(mac != 0)
    {
      mac->TraceDisconnectWithoutContext("TxErrHeader", m_nb.GetTxErrorCallback());
    }
  }

  // Close socket
  Ptr<Socket> socket = FindSocketWithInterfaceAddress(m_ipv4->GetAddress(interface, 0));
  NS_ASSERT(socket);
  socket->Close();
  m_socketAddresses.erase(socket);
  if(m_socketAddresses.empty())
  {
    NS_LOG_LOGIC("No maqr interfaces");
    m_nb.Clear();
    return;
  }
}

void RoutingProtocol::NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address)
{
  NS_LOG_FUNCTION(this << " interface " << interface << " address " << address);
  Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol>();
  if(!l3->IsUp(interface))
  {
    return;
  }
  if(l3->GetNAddresses(interface) == 1)
  {
    Ipv4InterfaceAddress iface = l3->GetAddress(interface, 0);
    Ptr<Socket> socket = FindSocketWithInterfaceAddress(iface);
    if(!socket)
    {
      if(iface.GetLocal() == Ipv4Address("127.0.0.1"))
      {
        return;
      }
      // Create a socket to listen only on the interface
      Ptr<Socket> socket = Socket::CreateSocket(GetObject<Node>(), UdpSocketFactory::GetTypeId());
      NS_ASSERT(socket != 0);
      socket->SetRecvCallback(MakeCallback(&RoutingProtocol::ReceiveHello, this));
      socket->BindToNetDevice(l3->GetNetDevice(interface));
      // Bind to any IP address so that broadcasts can be received
      socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), MAQR_PORT));
      socket->SetAllowBroadcast(true);
      m_socketAddresses.insert(std::make_pair(socket, iface));

      Ptr<NetDevice> dev = m_ipv4->GetNetDevice(m_ipv4->GetInterfaceForAddress(iface.GetLocal()));
    }
  }
  else
  {
    NS_LOG_LOGIC("MAQR does not work with more than one address per each interface. Ignore added address");
  }
}

void RoutingProtocol::NotifyRemoveAddress(uint32_t i, Ipv4InterfaceAddress address)
{
  NS_LOG_FUNCTION(this);
  Ptr<Socket> socket = FindSocketWithInterfaceAddress(address);
  if(socket)
  {
    m_socketAddresses.erase(socket);
    Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol>();
    if(l3->GetNAddresses(i))
    {
      Ipv4InterfaceAddress iface = l3->GetAddress(i, 0);
      // Create a socket to listen only on this interface
      Ptr<Socket> socket = Socket::CreateSocket(GetObject<Node>(), UdpSocketFactory::GetTypeId());
      NS_ASSERT(socket != 0);
      socket->SetRecvCallback(MakeCallback(&RoutingProtocol::ReceiveHello, this));
      // Bind to any IO address so that broadcasts can be received
      socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), MAQR_PORT));
      socket->SetAllowBroadcast(true);
      m_socketAddresses.insert(std::make_pair(socket, iface));

      Ptr<NetDevice> dev = m_ipv4->GetNetDevice(m_ipv4->GetInterfaceForAddress(iface.GetLocal()));
    }
    if(m_socketAddresses.empty())
    {
      NS_LOG_LOGIC("No maqr interface");
      m_nb.Clear();
      return;
    }
  }
  else
  {
    NS_LOG_LOGIC("Remove address not participating in MAQR operation");
  }
}

/**
 * \TODO: SetIpv4 sets routing protocol attrubutes and schedules Start()
 */
void RoutingProtocol::SetIpv4(Ptr<Ipv4> ipv4)
{
  NS_ASSERT(ipv4 != 0);
  NS_ASSERT(m_ipv4 == 0);
  m_ipv4 = ipv4;

  Simulator::ScheduleNow(&RoutingProtocol::Start, this);
}

Ptr<Ipv4Route> RoutingProtocol::LoopbackRoute(const Ipv4Header &hdr, Ptr<NetDevice> oif) const
{
  NS_LOG_FUNCTION (this << hdr);
  NS_ASSERT(m_lo != 0);
  Ptr<Ipv4Route> rt = Create<Ipv4Route>();
  rt->SetDestination(hdr.GetDestination());
  auto j = m_socketAddresses.cbegin();
  if(oif)
  {
    // Iterate to find an address on the oif device
    for(j = m_socketAddresses.cbegin(); j != m_socketAddresses.cend(); ++j)
    {
      Ipv4Address addr = j->second.GetLocal();
      int32_t interface = m_ipv4->GetInterfaceForAddress(addr);
      if(oif == m_ipv4->GetNetDevice(static_cast<uint32_t>(interface)))
      {
        rt->SetSource(addr);
        break;
      }
    }
  }
  else
  {
    rt->SetSource(j->second.GetLocal());
  }
  NS_ASSERT_MSG(rt->GetSource() != Ipv4Address(), "Valid MAQR source address not found");
  rt->SetGateway(Ipv4Address("127.0.0.1"));
  rt->SetOutputDevice(m_lo);
  return rt;
}

uint32_t RoutingProtocol::GetProtocolNumber(void) const
{
  return MAQR_PORT;
}

Ptr<Socket> RoutingProtocol::FindSocketWithInterfaceAddress(Ipv4InterfaceAddress addr) const
{
  NS_LOG_FUNCTION(this << addr);
  for(auto j = m_socketAddresses.cbegin(); j != m_socketAddresses.cend(); ++j)
  {
    Ptr<Socket> socket = j->first;
    Ipv4InterfaceAddress iface = j->second;
    if(iface == addr)
    {
      return socket;
    }
  }
  Ptr<Socket> socket;
  return socket;
}

void RoutingProtocol::Start()
{
  m_scb = MakeCallback(&RoutingProtocol::Send, this);
  m_ecb = MakeCallback(&RoutingProtocol::Drop, this);
  m_helloIntervalTimer.SetFunction(&RoutingProtocol::HelloTimerExpire, this);
  m_helloIntervalTimer.Schedule(Seconds(1));

  m_mobility = this->GetObject<Node>()->GetObject<MobilityModel>();

  m_selfIpv4Address = m_ipv4->GetAddress(1, 0).GetLocal();
}

void RoutingProtocol::ReceiveHello(Ptr<Socket> socket)
{
  NS_LOG_FUNCTION(this << socket);
  Address sourceAddress;
  Ptr<Packet> packet = socket->RecvFrom(sourceAddress); // read the packet source

  TypeHeader tHeader(MAQRTYPE_HELLO);
  packet->RemoveHeader(tHeader);
  if(!tHeader.IsValid())
  {
    NS_LOG_DEBUG("MAQR message " << packet->GetUid() << " with unknown type received: " << tHeader.Get() << ". Ignored");
    return;
  }
  HelloHeader hdr;
  packet->RemoveHeader(hdr);
  float maxQ;
  maxQ = hdr.GetqValue();
  Vector2D curPos;
  curPos = hdr.GetCurPosition();
  InetSocketAddress inetSourceAddr = InetSocketAddress::ConvertFrom(sourceAddress);
  Ipv4Address sender = inetSourceAddr.GetIpv4();
  Ipv4Address receiver = m_socketAddresses[socket].GetLocal();

  UpdateNeighbor(sender, maxQ, curPos);
}

void RoutingProtocol::UpdateNeighbor(Ipv4Address origin, float qValue, Vector2D pos)
{
  /**
   * \todo update neighbor entry
   */
  m_nb.AddEntry(origin, Neighbors::Neighbor(origin, ns3::Mac48Address(), Simulator::Now(), pos, 0.0, 0.0, 0.0));
}

void RoutingProtocol::SendHello()
{
  NS_LOG_FUNCTION(this);
  double positionX, positionY;

  Ptr<MobilityModel> mm = m_ipv4->GetObject<MobilityModel>();

  positionX = mm->GetPosition().x;
  positionY = mm->GetPosition().y;

  for(auto i = m_socketAddresses.cbegin(); i != m_socketAddresses.cend(); ++i)
  {
    Ptr<Socket> socket = i->first;
    Ipv4InterfaceAddress iface = i->second;
    HelloHeader helloHeader((uint8_t)0, (uint16_t)0, m_ipv4->GetAddress(1, 0).GetLocal(), 0.0, Vector2D(positionX, positionY));

    Ptr<Packet> packet = Create<Packet>();
    packet->AddHeader(helloHeader);
    TypeHeader tHeader(MAQRTYPE_HELLO);
    packet->AddHeader(tHeader);

    Ipv4Address destination;
    if(iface.GetMask() == Ipv4Mask::GetOnes())
    {
      destination = Ipv4Address("255.255.255.255");
    }
    else
    {
      destination = iface.GetBroadcast();
    }
    socket->SendTo(packet, 0, InetSocketAddress(destination, MAQR_PORT));
  }
}

void RoutingProtocol::HelloTimerExpire()
{
  SendHello();
  m_helloIntervalTimer.Cancel();
  m_helloIntervalTimer.Schedule(m_helloInterval);
}

bool RoutingProtocol::IsMyOwnAddress (Ipv4Address src)
{
  NS_LOG_FUNCTION(this << src);
  for(auto i = m_socketAddresses.cbegin(); i != m_socketAddresses.cend(); ++i)
  {
    Ipv4InterfaceAddress iface = i->second;
    if(src == iface.GetLocal())
    {
      return true;
    }
  }
  return false;
}

void RoutingProtocol::Send(Ptr<Ipv4Route> route, Ptr<const Packet> packet, const Ipv4Header &header)
{
  Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol>();
  NS_ASSERT(l3 != 0);
  Ptr<Packet> p = packet->Copy();
  l3->Send(p, route->GetSource(), header.GetDestination(), header.GetProtocol(), route);
}

void RoutingProtocol::Drop(Ptr<const Packet> packet, const Ipv4Header &header, Socket::SocketErrno err)
{
  NS_LOG_DEBUG(m_mainAddress << " drop packet" << packet->GetUid() << " to "
                             << header.GetDestination() << " from queue. Error" << err);
}

} // namespace maqr
} // namespace ns3