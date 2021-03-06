#ifndef MAQR_ROUTING_PROTOCOL_H
#define MAQR_ROUTING_PROTOCOL_H

#pragma once

#include "maqr-packet.h"
#include "maqr-rtable.h"
#include "maqr-rl-learning.h"
#include "maqr-neighbor.h"
#include "maqr-rqueue.h"

#include "ns3/node.h"
#include "ns3/random-variable-stream.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/mobility-model.h"
#include "ns3/controlled-random-waypoint-mobility-model.h"
#include "ns3/wifi-mac.h"

#include "ns3/timer.h"

#include <algorithm>
#include <deque>
#include <map>
#include <numeric>
#include <iomanip>
#include <string>

namespace ns3 {
namespace maqr {

/**
 * \ingroup maqr
 * \brief MAQR routing protocol
 */
class RoutingProtocol : public Ipv4RoutingProtocol
{
public:
  /**
   * \brief Get the type ID, set metadata
   * \return the object TypeId
   */
  static TypeId GetTypeId(void);
  static const uint32_t MAQR_PORT;

  RoutingProtocol();
  virtual ~RoutingProtocol();
  // Destructor implementation
  virtual void DoDispose();

  /**
   * \brief Query routing cache for an existing route, for an outbound packet
   *
   * This lookup is used by transport protocols.  It does not cause any
   * packet to be forwarded, and is synchronous.  Can be used for
   * multicast or unicast.  The Linux equivalent is ip_route_output()
   *
   * The header input parameter may have an uninitialized value
   * for the source address, but the destination address should always be 
   * properly set by the caller.
   *
   * \param p packet to be routed.  Note that this method may modify the packet.
   *          Callers may also pass in a null pointer. 
   * \param header input parameter (used to form key to search for the route)
   * \param oif Output interface Netdevice.  May be zero, or may be bound via
   *            socket options to a particular output interface.
   * \param sockerr Output parameter; socket errno 
   *
   * \returns a code that indicates what happened in the lookup
   */
  virtual Ptr<Ipv4Route> RouteOutput(Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, 
                                    Socket::SocketErrno &sockerr);
  
  /**
   * \brief Route an input packet (to be forwarded or locally delivered)
   *
   * This lookup is used in the forwarding process.  The packet is
   * handed over to the Ipv4RoutingProtocol, and will get forwarded onward
   * by one of the callbacks.  The Linux equivalent is ip_route_input().
   * There are four valid outcomes, and a matching callbacks to handle each.
   *
   * \param p received packet
   * \param header input parameter used to form a search key for a route
   * \param idev Pointer to ingress network device
   * \param ucb Callback for the case in which the packet is to be forwarded
   *            as unicast
   * \param mcb Callback for the case in which the packet is to be forwarded
   *            as multicast
   * \param lcb Callback for the case in which the packet is to be locally
   *            delivered
   * \param ecb Callback to call if there is an error in forwarding
   * \returns true if the Ipv4RoutingProtocol takes responsibility for 
   *          forwarding or delivering the packet, false otherwise
   */
  virtual bool RouteInput (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
                           UnicastForwardCallback ucb, MulticastForwardCallback mcb, 
                           LocalDeliverCallback lcb, ErrorCallback ecb);
  
  /**
   * \param interface the index of the interface we are being notified about
   *
   * Protocols are expected to implement this method to be notified of the state change of
   * an interface in a node.
   */
  virtual void NotifyInterfaceUp (uint32_t interface);
  /**
   * \param interface the index of the interface we are being notified about
   *
   * Protocols are expected to implement this method to be notified of the state change of
   * an interface in a node.
   */
  virtual void NotifyInterfaceDown (uint32_t interface);

  /**
   * \param interface the index of the interface we are being notified about
   * \param address a new address being added to an interface
   *
   * Protocols are expected to implement this method to be notified whenever
   * a new address is added to an interface. Typically used to add a 'network route' on an
   * interface. Can be invoked on an up or down interface.
   */
  virtual void NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address);

  /**
   * \param interface the index of the interface we are being notified about
   * \param address a new address being added to an interface
   *
   * Protocols are expected to implement this method to be notified whenever
   * a new address is removed from an interface. Typically used to remove the 'network route' of an
   * interface. Can be invoked on an up or down interface.
   */
  virtual void NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address);

  /**
   * \param ipv4 the ipv4 object this routing protocol is being associated with
   * 
   * Typically, invoked directly or indirectly from ns3::Ipv4::SetRoutingProtocol
   */
  virtual void SetIpv4 (Ptr<Ipv4> ipv4);
  // Create loopback route for given header. route entry dest: hdr's dest, gateway:127.0.0.1, source:local
  Ptr<Ipv4Route> LoopbackRoute(const Ipv4Header &hdr, Ptr<NetDevice> oif) const;
  // Get Protocol number
  uint32_t GetProtocolNumber(void) const;
  // m_socketAddresses maps socket->InterfaceAddress, reverse find socket with interface address
  Ptr<Socket> FindSocketWithInterfaceAddress(Ipv4InterfaceAddress addr) const;
  // Find subnet directed broadcast socket with local interface address iface
  Ptr<Socket> FindSubnetBroadcastSocketWithInterfaceAddress (Ipv4InterfaceAddress addr ) const;


  /**
   * \brief Start protocol operation
   */
  void Start();

  virtual void DoInitialize (void);

  /**
   * \brief Process Hello packet, then update neighbor table
   */
  virtual void ReceiveHello (Ptr<Packet> p, Ipv4Address receiver, Ipv4Address sender);
  /**
   * \brief Update neighbor table
   */
  virtual void UpdateNeighbor (Ipv4Address origin, float qValue, Vector2D pos);
  /**
   * \brief Send Hello packet
   */
  virtual void SendHello ();
  /**
   * Send packet to destination scoket
   * \param socket - destination node socket
   * \param packet - packet to send
   * \param destination - destination node IP address
   */
  void SendTo (Ptr<Socket> socket, Ptr<Packet> packet, Ipv4Address destination);
  /**
   * \brief Schedule Hello timer
   */
  void HelloTimerExpire();
  /**
   * \brief Judge whether origin is one of its own Ipv4Address
   */
  virtual bool IsMyOwnAddress (Ipv4Address origin);

  void Send (Ptr<Ipv4Route>, Ptr<const Packet>, const Ipv4Header &);
  /// Notify that packet is dropped for some reason
  void Drop (Ptr<const Packet>, const Ipv4Header &, Socket::SocketErrno);
  /**
   * \brief If route exists and valid, forward packet.
   * \todo process rl-learning 
   */
  bool Forwarding (Ptr<const Packet> p, const Ipv4Header& header, UnicastForwardCallback ucb, ErrorCallback ecb);

  // Get the max Q-value from hop to target, the value stores in other nodes
  float GetMaxNextStateQValue (Ipv4Address hop, Ipv4Address target);

  // Get node by searching Ipv4Address in NetDevice in each node
  Ptr<Node> GetNodeWithAddress (Ipv4Address address);

  float UpdateQValue(Ipv4Address target, Ipv4Address hop, RewardType type);

  virtual void PrintRoutingTable (ns3::Ptr<ns3::OutputStreamWrapper>, Time::Unit unit = Time::S) const
  {
    return;
  }

  int64_t AssignStreams (int64_t stream);

  // Receive and process control packet
  void RecvMaqr (Ptr<Socket> socket);

  /**
   * \brief Notify that an MPDU was dropped
   * \todo
   */
  void NotifyTxError (WifiMacDropReason reason, Ptr<const WifiMacQueueItem> mpdu);

  // IP protocol
  Ptr<Ipv4> m_ipv4;
  // Raw unicast socket per each IP interface, map socket ->iface address (IP + mask)
  std::map<Ptr<Socket>, Ipv4InterfaceAddress> m_socketAddresses;
  /// Raw subnet directed broadcast socket per each IP interface, map socket -> iface address (IP + mask)
  std::map< Ptr<Socket>, Ipv4InterfaceAddress > m_socketSubnetBroadcastAddresses;
  // Loopback device used to defer route request until a route is found
  Ptr<NetDevice> m_lo;
  // Nodes IP address
  Ptr<NetDevice> m_mainAddress;
  // The node's IP address
  Ipv4Address m_selfIpv4Address;
  // Routing Table
  RoutingTable m_routingTable;
  // Neighbor table
  Neighbors m_nb;

  // A "drop-front" queue used by the routing layer to buffer packets to which it does not have a route
  RequestQueue m_queue;
  uint32_t m_maxQueueLen;
  Time m_maxQueueTime;

  // Unicast callbackfor own packets
  UnicastForwardCallback m_scb;
  // Error callback for own packets;
  ErrorCallback m_ecb;

  // Q-Learning algorithm
  QLearning m_qLearning;
  // Hello interval
  Time m_helloInterval;
  // Hello timer
  Timer m_helloIntervalTimer;
  // uniform random variable
  Ptr<UniformRandomVariable> m_uniformRandomVariable;

  // Pointer to mobility handler
  Ptr<MobilityModel> m_mobility;

};

} // namespace maqr
} // namespace ns3

#endif // MAQR_ROUTING_PROTOCOL_H