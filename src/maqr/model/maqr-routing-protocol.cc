#include "maqr-routing-protocol.h"

namespace ns3 {
namespace maqr {

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
  Ipv4Address receiver = m_socketAddressed[socket].GetLocal();

  UpdateNeighbor(sender, maxQ, curPos);
}

} // namespace maqr
} // namespace ns3