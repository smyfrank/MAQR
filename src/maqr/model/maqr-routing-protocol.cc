#include "maqr-routing-protocol.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("MaqrRoutingProtocol");

namespace maqr {

NS_OBJECT_ENSURE_REGISTERED(RoutingProtocol);

const uint32_t RoutingProtocol::MAQR_PORT = 5678;

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