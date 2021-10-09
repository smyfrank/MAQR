//
// Created by smy on 2021/10/6.
//

#ifndef MAQR_RQUEUE_H
#define MAQR_RQUEUE_H

#include <vector>
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/simulator.h"

namespace ns3 {
namespace maqr {

/**
 * \ingroup maqr
 * \brief MAQR Queue Entry
 */
class QueueEntry
{
public:
  typedef Ipv4RoutingProtocol::UnicastForwardCallback UnicastForwardCallback;
  typedef Ipv4RoutingProtocol::ErrorCallback ErrorCallback;

  /**
   * constructor
   * \param pa the packet to add to the queue
   * \param h the Ipv4Header
   * \param ucb the UnicastForwardCallback function
   * \param ecb the ErrorCallback function
   * \param exp the expiration time
   */
  QueueEntry(Ptr<const Packet> pa = 0, Ipv4Header const & h = Ipv4Header(),
              UnicastForwardCallback ucb = UnicastForwardCallback(),
              ErrorCallback ecb = ErrorCallback(), Time exp = Simulator::Now())
      : m_packet(pa),
        m_header(h),
        m_ucb(ucb),
        m_ecb(ecb),
        m_expire(exp + Simulator::Now())
  {
  }

  /**
   * \brief Compare queue entries
   * \param o QueueEntry to compare
   * \return true if equal
   */
  bool operator== (QueueEntry const& o) const
  {
    return ((m_packet == o.m_packet) && (m_header.GetDestination() == o.m_header.GetDestination())
              && (m_expire == o.m_expire));
  }

  // Fields
  /**
   * Get unicast forward callback
   * \returns unicast callback
   */
  UnicastForwardCallback  GetUnicastForwardCallback() const
  {
    return m_ucb;
  }
  /**
   * Set unicast forward callback
   * \param ucb The unicast callback
   */
  void SetUnicastForwardCallback(UnicastForwardCallback ucb)
  {
    m_ucb = ucb;
  }
  /**
   * Get Error callback
   * \returns the error callback
   */
  ErrorCallback GetErrorCallback() const
  {
    return m_ecb;
  }
  /**
   * Set error callback
   * \param ecb The error callback
   */
  void SetErrorCallback(ErrorCallback ecb)
  {
    m_ecb = ecb;
  }
  /**
   * Get packet from entry
   * \returns the packet
   */
  Ptr<const Packet> GetPacket() const
  {
    return m_packet;
  }
  /**
   * Set packet in entry
   * \param p The packet
   */
  void SetPacket(Ptr<const Packet> p)
  {
    m_packet = p;
  }
  /**
   * Get IPv4 header
   * \returns the IPv4 header
   */
  Ipv4Header GetIpv4Header() const
  {
    return m_header;
  }
  /**
   * Set IPv4 header
   * \param h the IPv4 header
   */
  void SetIpv4Header(Ipv4Header h)
  {
    m_header = h;
  }
  /**
   * Set expire time
   * \param exp The expiration time
   */
  void SetExpireTime(Time exp)
  {
    m_expire = exp + Simulator::Now();
  }
  /**
   * Get expire time
   * \returns the expiration time
   */
  Time GetExpireTime() const
  {
    return m_expire - Simulator::Now();
  }

private:
  /// Data packet
  Ptr<const Packet> m_packet;
  /// IP header
  Ipv4Header m_header;
  /// Unicast forward callback
  UnicastForwardCallback m_ucb;
  /// Error callback
  ErrorCallback m_ecb;
  /// Expire time for queue entry
  Time m_expire;
};
} // namespace maqr
} // namespace ns3

#endif //MAQR_RQUEUE_H
