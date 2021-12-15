#define NS_LOG_APPEND_CONTEXT                                   \
  if (m_ipv4) { std::clog << "[node " << m_ipv4->GetObject<Node> ()->GetId () << "] "; }

#include "maqr-routing-protocol.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/uinteger.h"
#include "ns3/wifi-net-device.h"
#include "ns3/adhoc-wifi-mac.h"
#include "ns3/socket.h"

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
  return tid;
}

RoutingProtocol::RoutingProtocol()
  : m_queue(m_maxQueueLen, m_maxQueueTime),
    m_maxQueueLen(64),
    m_maxQueueTime(Seconds(30)),
    m_qLearning(0.3, 0.9, 0.3, Seconds(1.1)),
    m_helloInterval(Seconds(1)),
    m_helloIntervalTimer(Timer::CANCEL_ON_DESTROY)
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
                                            Socket::SocketErrno &sockerr)
{
  NS_LOG_FUNCTION (this << "packet " << p->GetUid () << " source " << header.GetSource () << " destination " << header.GetDestination ()
                  << " NetDevice " << (oif ? oif->GetIfIndex () : 0));
  if (!p)
  {
    NS_LOG_DEBUG ("Packet is == 0");
    return LoopbackRoute (header, oif);
  }
  if (m_socketAddresses.empty ())
  {
    sockerr = Socket::ERROR_NOROUTETOHOST;
    NS_LOG_LOGIC ("No maqr interfaces");
    Ptr<Ipv4Route> route;
    return route;
  }

  // Generate Ipv4Route
  sockerr = Socket::ERROR_NOTERROR;
  Ptr<Ipv4Route> route = Create<Ipv4Route> ();
  Ipv4Address dst = header.GetDestination ();
  Ipv4Address nextHop;
  m_nb.Purge ();
  if (m_nb.IsNeighbor (dst))
  {
    nextHop = dst;
  }
  else
  {
    std::set<Ipv4Address> activeNeighbors = m_nb.GetAllActiveNeighbors ();

    /**
     * \todo loop back if there's no active neighbors
     * \brief Drop packet if no active neighbors
     */
    if (activeNeighbors.empty ())
    {
      return LoopbackRoute(header, oif);
    }

    nextHop = m_qLearning.GetNextHop (dst, activeNeighbors);
  }
  if (nextHop != Ipv4Address::GetZero ())
  {
    NS_LOG_DEBUG ("Destination: " << dst);
    route->SetDestination (dst);
    if (header.GetSource () == Ipv4Address ("102.102.102.102"))
    {
      route->SetSource (m_ipv4->GetAddress (1, 0).GetLocal ());
    }
    else
    {
      route->SetSource (header.GetSource ());
    }
    route->SetGateway (nextHop);
    route->SetOutputDevice (m_ipv4->GetNetDevice (1)); // TODO
    NS_ASSERT (route != 0);
    NS_LOG_DEBUG ("Exist route to " << route->GetDestination () << " from source " << route->GetSource ());
    if (oif != 0 && route->GetOutputDevice () != oif)
    {
      NS_LOG_DEBUG ("Output device doesn't match. Dropped.");
      sockerr = Socket::ERROR_NOROUTETOHOST;
      return Ptr<Ipv4Route> ();
    }
    return route;
  }

  /**
    Valid route not found, in this case we return loopback.
    Actual route request will be deferred until packet will be fully formed,
    routed to loopback, received from Loobback and passed to RouteInput (see below)
    \TODO: DeferredRouteOutputTag0
  */
  NS_LOG_DEBUG ("Valid Route not found");
  return LoopbackRoute(header, oif);

}

bool RoutingProtocol::RouteInput(Ptr<const Packet> p, const Ipv4Header &header,
                                 Ptr<const NetDevice> idev, UnicastForwardCallback ucb,
                                 MulticastForwardCallback mcb, LocalDeliverCallback lcb,
                                 ErrorCallback ecb)
{
  NS_LOG_FUNCTION (this << "Packet " << p->GetUid () << " source " << header.GetSource () << " dest " << header.GetDestination() << idev->GetAddress ());
  if (m_socketAddresses.empty ())
  {
    NS_LOG_LOGIC ("No maqr interfaces");
    return false;
  }
  NS_ASSERT (m_ipv4 != 0);
  NS_ASSERT (p != 0);
  // Check if input device supports IP
  NS_ASSERT (m_ipv4->GetInterfaceForDevice (idev) >= 0);
  int32_t iif = m_ipv4->GetInterfaceForDevice (idev);
  Ipv4Address dst = header.GetDestination ();
  Ipv4Address origin = header.GetSource ();

  // the packet will be dropped silently if the source of input packet is this node itself
  // comment it since MAQR could bear this kind of loop
  /*
  if (IsMyOwnAddress (origin))
  {
    return true;
  }
  */
  

  if (dst.IsMulticast ())
  {
    return false;
  }

  // Broadcast local delivery/forward
  for (auto j = m_socketAddresses.cbegin (); j != m_socketAddresses.cend (); ++j)
  {
    Ipv4InterfaceAddress iface = j->second;
    if (m_ipv4->GetInterfaceForAddress (iface.GetLocal ()) == iif)
    {
      if (dst == iface.GetBroadcast () || dst.IsBroadcast ())
      {
        Ptr<Packet> packet = p->Copy ();
        if (lcb.IsNull () == false)
        {
          NS_LOG_LOGIC ("Broadcast local delivery to " << iface.GetLocal ());
          lcb (p, header, iif);
          // Fall through to additional processing
        }
        else
        {
          NS_LOG_ERROR ("Unable to deliver packet locally due to null callback "
                        << p->GetUid () << " from " << origin);
          ecb (p, header, Socket::ERROR_NOROUTETOHOST);
        }
        if (header.GetTtl () > 1)
        {
          NS_LOG_LOGIC ("Forward broadcast. TTL " << (uint16_t) header.GetTtl ());
          /**
           * \TODO: broadcast forward
           */
        }
        else
        {
          NS_LOG_DEBUG ("TTL exceeded. Drop packet " << p->GetUid ());
        }
        return true;
      }
    }
  }

  // Unicast local delivery
  if (m_ipv4->IsDestinationAddress (dst, iif))
  {
    if (lcb.IsNull () == false)
    {
      NS_LOG_LOGIC ("Unicast local delivery to " << dst);
      lcb (p, header, iif);
    }
    else
    {
      NS_LOG_ERROR ("Unable to deliver packet locally due to null callback "
                    << p->GetUid () << " from" << origin);
      ecb (p, header, Socket::ERROR_NOROUTETOHOST);
    }
    return true;
  }

  // Check if input device supports IP forwarding
  if (m_ipv4->IsForwarding (iif) == false)
  {
    NS_LOG_LOGIC ("Forwarding disable for this interface");
    ecb (p, header, Socket::ERROR_NOROUTETOHOST);
    return true;
  }

  // Unicast forward packet
  return Forwarding (p, header, ucb, ecb);
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
  socket->SetRecvCallback(MakeCallback(&RoutingProtocol::RecvMaqr, this));

  socket->BindToNetDevice(l3->GetNetDevice(interface));  
  socket->Bind(InetSocketAddress(iface.GetLocal (), MAQR_PORT));
  socket->SetAllowBroadcast(true);
  socket->SetIpRecvTtl (true);
  m_socketAddresses.insert(std::make_pair(socket, iface));

  // create also a subnet broadcast socket
  socket = Socket::CreateSocket (GetObject<Node> (),
                                 UdpSocketFactory::GetTypeId ());
  NS_ASSERT (socket != 0);
  socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvMaqr, this));
  socket->BindToNetDevice (l3->GetNetDevice (interface));
  socket->Bind (InetSocketAddress (iface.GetBroadcast (), MAQR_PORT));
  socket->SetAllowBroadcast (true);
  socket->SetIpRecvTtl (true);
  m_socketSubnetBroadcastAddresses.insert (std::make_pair (socket, iface));

  // Add local broadcast record to the routing table
  Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (iface.GetLocal ()));
  RoutingTableEntry rt (/*device=*/ dev, /*dst=*/ iface.GetBroadcast (), /*iface=*/ iface,
                        /*next hop=*/ iface.GetBroadcast (), /*lifetime=*/ Simulator::GetMaximumSimulationTime ());
  m_routingTable.AddRoute (rt);

  /*
  if (l3->GetInterface (i)->GetArpCache ())
  {
    m_nb.AddArpCache (l3->GetInterface (i)->GetArpCache ());
  }
  */

  // Allow neighbor manager use this interface for layer 2 feedback if possible
  Ptr<WifiNetDevice> wifi = dev->GetObject<WifiNetDevice> ();
  if(wifi == 0)
  {
    return;
  }
  Ptr<WifiMac> mac = wifi->GetMac();
  if(mac == 0)
  {
    return;
  }
  
  mac->TraceConnectWithoutContext ("DroppedMpdu", MakeCallback (&RoutingProtocol::NotifyTxError, this));
}

void RoutingProtocol::NotifyTxError (WifiMacDropReason reason, Ptr<const WifiMacQueueItem> mpdu)
{
  m_nb.GetTxErrorCallback ()(mpdu->GetHeader ());
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
      mac->TraceDisconnectWithoutContext ("DroppedMpdu",
                                          MakeCallback (&RoutingProtocol::NotifyTxError, this));
      // m_nb.DelArpCache (l3->GetInterface (interface)->GetArpCache ());
    }
  }

  // Close socket
  Ptr<Socket> socket = FindSocketWithInterfaceAddress(m_ipv4->GetAddress(interface, 0));
  NS_ASSERT(socket);
  socket->Close();
  m_socketAddresses.erase(socket);

  socket = FindSubnetBroadcastSocketWithInterfaceAddress (m_ipv4->GetAddress (interface, 0));
  NS_ASSERT (socket);
  socket->Close ();
  m_socketSubnetBroadcastAddresses.erase (socket);

  if(m_socketAddresses.empty())
  {
    NS_LOG_LOGIC("No maqr interfaces");
    m_nb.Clear();
    m_routingTable.Clear ();
    return;
  }
  m_routingTable.DeleteAllRoutesFromInterface (m_ipv4->GetAddress (interface, 0));
}

Ptr<Socket>
RoutingProtocol::FindSubnetBroadcastSocketWithInterfaceAddress (Ipv4InterfaceAddress addr ) const
{
  NS_LOG_FUNCTION (this << addr);
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
         m_socketSubnetBroadcastAddresses.begin (); j != m_socketSubnetBroadcastAddresses.end (); ++j)
    {
      Ptr<Socket> socket = j->first;
      Ipv4InterfaceAddress iface = j->second;
      if (iface == addr)
        {
          return socket;
        }
    }
  Ptr<Socket> socket;
  return socket;
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
      socket->SetRecvCallback(MakeCallback(&RoutingProtocol::RecvMaqr, this));
      socket->BindToNetDevice(l3->GetNetDevice(interface));
      socket->Bind(InetSocketAddress(iface.GetLocal (), MAQR_PORT));
      socket->SetAllowBroadcast(true);
      m_socketAddresses.insert(std::make_pair(socket, iface));

      // create also a subnet directed broadcast socket
      socket = Socket::CreateSocket (GetObject<Node> (),
                                     UdpSocketFactory::GetTypeId ());
      NS_ASSERT (socket != 0);
      socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvMaqr, this));
      socket->BindToNetDevice (l3->GetNetDevice (interface));
      socket->Bind (InetSocketAddress (iface.GetBroadcast (), MAQR_PORT));
      socket->SetAllowBroadcast (true);
      socket->SetIpRecvTtl (true);
      m_socketSubnetBroadcastAddresses.insert (std::make_pair (socket, iface));

      // Add local broadcast record to the routing table
      Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (iface.GetLocal ()));
      RoutingTableEntry rt (/*device=*/ dev, /*dst=*/ iface.GetBroadcast (), /*iface=*/ iface,
                        /*next hop=*/ iface.GetBroadcast (), /*lifetime=*/ Simulator::GetMaximumSimulationTime ());
      m_routingTable.AddRoute (rt);
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
    m_routingTable.DeleteAllRoutesFromInterface (address);
    socket->Close ();
    m_socketAddresses.erase(socket);

    Ptr<Socket> unicastSocket = FindSubnetBroadcastSocketWithInterfaceAddress (address);
    if (unicastSocket)
    {
      unicastSocket->Close ();
      m_socketAddresses.erase (unicastSocket);
    }

    Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol>();
    if(l3->GetNAddresses(i))
    {
      Ipv4InterfaceAddress iface = l3->GetAddress(i, 0);
      // Create a socket to listen only on this interface
      Ptr<Socket> socket = Socket::CreateSocket(GetObject<Node>(), UdpSocketFactory::GetTypeId());
      NS_ASSERT(socket != 0);
      socket->SetRecvCallback(MakeCallback(&RoutingProtocol::RecvMaqr, this));
      // Bind to any IP address so that broadcasts can be received (AODV says so)
      socket->BindToNetDevice (l3->GetNetDevice (i));
      socket->Bind(InetSocketAddress(iface.GetLocal (), MAQR_PORT));
      socket->SetAllowBroadcast(true);
      socket->SetIpRecvTtl (true);
      m_socketAddresses.insert(std::make_pair(socket, iface));

      // create also a unicast socket (AODV says so)
      socket = Socket::CreateSocket (GetObject<Node> (),
                                     UdpSocketFactory::GetTypeId ());
      NS_ASSERT (socket != 0);
      socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvMaqr, this));
      socket->BindToNetDevice (l3->GetNetDevice (i));
      socket->Bind (InetSocketAddress (iface.GetBroadcast (), MAQR_PORT));
      socket->SetAllowBroadcast (true);
      socket->SetIpRecvTtl (true);
      m_socketSubnetBroadcastAddresses.insert (std::make_pair (socket, iface));

      // Add local broadcast record to the routing table
      Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (iface.GetLocal ()));
      RoutingTableEntry rt (/*device=*/ dev, /*dst=*/ iface.GetBroadcast (), /*iface=*/ iface,
                        /*next hop=*/ iface.GetBroadcast (), /*lifetime=*/ Simulator::GetMaximumSimulationTime ());
      m_routingTable.AddRoute (rt);
    }
    if(m_socketAddresses.empty())
    {
      NS_LOG_LOGIC("No maqr interface");
      m_nb.Clear();
      m_routingTable.Clear ();
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

  // Create lo route. It is asserted that the only one interface up for now is loopback
  NS_ASSERT (m_ipv4->GetNInterfaces () == 1 && m_ipv4->GetAddress (0, 0).GetLocal () == Ipv4Address ("127.0.0.1"));
  m_lo = m_ipv4->GetNetDevice (0);
  NS_ASSERT (m_lo != 0);

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
  NS_LOG_FUNCTION (this);
  m_scb = MakeCallback(&RoutingProtocol::Send, this);
  m_ecb = MakeCallback(&RoutingProtocol::Drop, this);
  m_helloIntervalTimer.SetFunction(&RoutingProtocol::HelloTimerExpire, this);
  m_helloIntervalTimer.Schedule(Seconds(1));

  m_mobility = this->GetObject<Node>()->GetObject<MobilityModel>();


}

void RoutingProtocol::DoInitialize (void)
{
  NS_LOG_FUNCTION (this);
  Ipv4RoutingProtocol::DoInitialize ();
}

void RoutingProtocol::RecvMaqr (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Address sourceAddress;
  Ptr<Packet> packet = socket->RecvFrom (sourceAddress);
  InetSocketAddress inetSourceAddr = InetSocketAddress::ConvertFrom (sourceAddress);
  Ipv4Address sender = inetSourceAddr.GetIpv4 ();
  Ipv4Address receiver;

  if (m_socketAddresses.find (socket) != m_socketAddresses.end ())
  {
    receiver = m_socketAddresses[socket].GetLocal ();
  }
  else if (m_socketSubnetBroadcastAddresses.find (socket) != m_socketSubnetBroadcastAddresses.end ())
  {
    receiver = m_socketSubnetBroadcastAddresses[socket].GetLocal ();
  }
  else
  {
    NS_ASSERT_MSG (false, "Received a packet from an unknown socket");
  }
  NS_LOG_DEBUG ("MAQR node " << this << " received a MAQR packet from " << sender << " to " << receiver);

  TypeHeader tHeader (MAQRTYPE_HELLO);
  packet->RemoveHeader (tHeader);
  if (!tHeader.IsValid ())
  {
    NS_LOG_DEBUG ("MAQR message " << packet->GetUid () << " with unknown type received: " << tHeader.Get () << ". Drop");
    return;
  }
  switch (tHeader.Get ())
  {
    case MAQRTYPE_HELLO:
    {
      ReceiveHello (packet, receiver, sender);
      break;
    }
  }
}

void RoutingProtocol::ReceiveHello(Ptr<Packet> p, Ipv4Address receiver, Ipv4Address sender)
{
  NS_LOG_FUNCTION(this << " src " << sender);
  HelloHeader hdr;
  p->RemoveHeader(hdr);
  float maxQ;
  maxQ = hdr.GetqValue();
  Vector2D curPos;
  curPos = hdr.GetCurPosition();

  UpdateNeighbor(sender, maxQ, curPos);
}

void RoutingProtocol::UpdateNeighbor(Ipv4Address origin, float qValue, Vector2D pos)
{
  NS_LOG_FUNCTION (this << "Update neighbor : " << origin);
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
    HelloHeader helloHeader((uint8_t)0, (uint16_t)0, iface.GetLocal(), 0.0, Vector2D(positionX, positionY));

    Ptr<Packet> packet = Create<Packet>();
    SocketIpTtlTag tag;
    tag.SetTtl (1);
    packet->AddPacketTag (tag);
    packet->AddHeader(helloHeader);
    TypeHeader tHeader(MAQRTYPE_HELLO);
    packet->AddHeader(tHeader);

    // Send to all-hosts broadcast if on /32 addr, subnet-directed otherwise
    Ipv4Address destination;
    if(iface.GetMask() == Ipv4Mask::GetOnes())
    {
      destination = Ipv4Address("255.255.255.255");
    }
    else
    {
      destination = iface.GetBroadcast();
    }
    Time jitter = Time (MilliSeconds (m_uniformRandomVariable->GetInteger (0, 10)));
    Simulator::Schedule (jitter, &RoutingProtocol::SendTo, this, socket, packet, destination);
  }
}

void
RoutingProtocol::SendTo (Ptr<Socket> socket, Ptr<Packet> packet, Ipv4Address destination)
{
  NS_LOG_FUNCTION (this << "Send packet " << packet->GetUid () << " to " << destination);
  socket->SendTo (packet, 0, InetSocketAddress (destination, MAQR_PORT));
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
      NS_LOG_DEBUG (src << " is own address");
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
  NS_LOG_FUNCTION(this << " drop packet " << packet->GetUid() << " from " << header.GetSource () << " to "
                             << header.GetDestination() << ". Error " << err);
}

bool RoutingProtocol::Forwarding (Ptr<const Packet> packet, const Ipv4Header& header,
                                  UnicastForwardCallback ucb, ErrorCallback ecb)
{
  NS_LOG_FUNCTION (this);
  Ipv4Address dst = header.GetDestination ();
  Ipv4Address origin = header.GetSource ();
  m_nb.Purge ();
  // m_qLearning.Purge ();
  RoutingTableEntry toDst;
  std::set<Ipv4Address> activeNeighbors = m_nb.GetAllActiveNeighbors ();

  /**
   * \todo loop back if there's no active neighbors
   * \brief Drop packet if no active neighbors
   */
  if (activeNeighbors.empty ())
  {
    NS_LOG_DEBUG ("There is no active neighbors. Drop");
    Drop (packet, header, Socket::ERROR_NOROUTETOHOST);
    return false;
  }

  NS_LOG_DEBUG ("Current active neighbors: ");
  for (auto i = activeNeighbors.cbegin (); i != activeNeighbors.cend (); ++i)
  {
    NS_LOG_DEBUG (*i << " ");
  }

  Ipv4Address nextHop = m_qLearning.GetNextHop (dst, activeNeighbors);
  if (nextHop != Ipv4Address::GetZero ())
  {
    Ptr<NetDevice> oif = m_ipv4->GetObject<NetDevice> ();
    Ptr<Ipv4Route> route = Create<Ipv4Route> ();
    route->SetDestination (dst);
    route->SetSource (origin);
    route->SetGateway (nextHop);
    route->SetOutputDevice (m_ipv4->GetNetDevice (1));
    NS_ASSERT (route != 0);
    NS_LOG_DEBUG ("Exist route to " << route->GetDestination () << " from interface" << route->GetOutputDevice ());
    NS_LOG_LOGIC (m_ipv4->GetAddress (1, 0).GetLocal () << " is forwarding packet " << packet->GetUid () << " to " << dst
                  << " from " << origin << " via nexthop neighbor " << nextHop);
    ucb (route, packet, header);

    // update Q-table
    if (m_nb.IsNeighbor (dst))
    {
      RewardType type = REACH_DESTINATION;
      UpdateQValue (dst, nextHop, type);
    }
    else
    {
      RewardType type = MIDWAY;
      UpdateQValue (dst, nextHop, type);
    }
    return true;
  }
  NS_LOG_LOGIC ("Drop packet " << packet->GetUid () << " as there is no nextHop to forward it.");
  return false;
}

Ptr<Node> RoutingProtocol::GetNodeWithAddress (Ipv4Address address)
{
  NS_LOG_FUNCTION (this << address);
  int32_t nNodes = NodeList::GetNNodes ();
  for (int32_t i = 0; i < nNodes; ++i)
    {
      Ptr<Node> node = NodeList::GetNode (i);
      Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
      int32_t ifIndex = ipv4->GetInterfaceForAddress (address);
      if (ifIndex != -1)
        {
          return node;
        }
    }
  return 0;
}

float RoutingProtocol::GetMaxNextStateQValue (Ipv4Address hop, Ipv4Address target)
{
  NS_LOG_FUNCTION (this << "Get max Q Value from next hop " << hop << " to " << target);
  Ptr<Node> nextNode = GetNodeWithAddress (hop);
  if (nextNode == 0)
  {
    NS_LOG_DEBUG ("Fail to get node with address " << hop);
    return 0.0;
  }
  Ptr<RoutingProtocol> routing = nextNode->GetObject<RoutingProtocol> ();
  if (routing == 0)
  {
    NS_LOG_DEBUG ("Fail to get routing protocol with node " << nextNode);
    return 0.0;
  }

  if(routing->m_qLearning.m_QTable.find (target) == routing->m_qLearning.m_QTable.end ())
  {
    return 0.0;
  }

  float result = 0.0;
  for(auto i = routing->m_qLearning.m_QTable.find (target)->second.cbegin (); i != routing->m_qLearning.m_QTable.find (target)->second.cend (); ++i)
  {
    if (i->second->GetqValue () > result)
    {
      result = i->second->GetqValue ();
    }
  }
  return result;
}

float RoutingProtocol::UpdateQValue(Ipv4Address target, Ipv4Address hop, RewardType type)
{
  // First get max Q-value amont the actions from hop to target
  NS_LOG_FUNCTION (this << "Update Q value via " << hop << " to " << target << ". Reward type: " << type);
  float maxQ = GetMaxNextStateQValue (hop, target);

  float newQValue = (1 - m_qLearning.m_learningRate) * m_qLearning.m_QTable.at(target).at(hop)->GetqValue() + 
          m_qLearning.m_learningRate * (m_qLearning.GetReward(hop, type) + m_qLearning.m_discoutRate * maxQ);
  m_qLearning.m_QTable.find (target)->second.find (hop)->second->SetqValue (newQValue);
  return newQValue;
}

int64_t RoutingProtocol::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_uniformRandomVariable->SetStream (stream);
  return 1;
}

} // namespace maqr
} // namespace ns3