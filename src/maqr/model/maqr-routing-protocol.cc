#include "maqr-routing-protocol.h"

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
    m_qLearning(0.3, 0.9, 0.3)
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

void RoutingProtocol::Start()
{
  m_scb = MakeCallback(&RoutingProtocol::Send, this);
  m_ecb = MakeCallback(&RoutingProtocol::Drop, this);
  m_helloIntervalTimer.SetFunction(&RoutingProtocol::SendHello, this);
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

} // namespace maqr
} // namespace ns3