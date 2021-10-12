#include "maqr-rtable.h"
#include "ns3/simulator.h"
#include <iomanip>
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("MaqrRoutingTable");

namespace maqr {
RoutingTableEntry::RoutingTableEntry(Ptr<NetDevice> dev, Ipv4Address dst,
                                    Ipv4InterfaceAddress iface, Ipv4Address nextHop,
                                    Time expiryTime)
  : m_expiryTime(expiryTime),
    m_iface(iface)
{
  m_ipv4Route = Create<Ipv4Route>();
  m_ipv4Route->SetDestination(dst);
  m_ipv4Route->SetGateway(nextHop);
  m_ipv4Route->SetSource(m_iface.GetLocal());
  m_ipv4Route->SetOutputDevice(dev);
}
RoutingTableEntry::~RoutingTableEntry ()
{
}
RoutingTable::RoutingTable()
{
}

bool RoutingTable::LookupRoute(Ipv4Address id, RoutingTableEntry& rt)
{
  if(m_ipv4AddressEntry.empty())
  {
    return false;
  }
  std::map<Ipv4Address, RoutingTableEntry>::const_iterator i = m_ipv4AddressEntry.find(id);
  if(i == m_ipv4AddressEntry.end())
  {
    return false;
  }
  rt = i->second;
  return true;
}

bool RoutingTable::LookupRoute(Ipv4Address id, RoutingTableEntry &rt, bool forRouteInput)
{
  if(m_ipv4AddressEntry.empty())
  {
    return false;
  }
  std::map<Ipv4Address, RoutingTableEntry>::const_iterator i = m_ipv4AddressEntry.find(id);
  if(i == m_ipv4AddressEntry.end())
  {
    return false;
  }
  if(forRouteInput == true && id == i->second.GetInterface().GetBroadcast())
  {
    return false;
  }
  rt = i->second;
  return true;
}

} // namespace maqr
} // namespace ns3