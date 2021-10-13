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

bool RoutingTable::DeleteRoute(Ipv4Address dst)
{
  if(m_ipv4AddressEntry.erase(dst) != 0)
  {
    NS_LOG_DEBUG("Route erased");
    return true;
  }
  return false;
}

uint32_t RoutingTable::RoutingTableSize()
{
  return m_ipv4AddressEntry.size();
}

bool RoutingTable::AddRoute(RoutingTableEntry& rt)
{
  std::pair<std::map<Ipv4Address, RoutingTableEntry>::iterator, bool> result =
    m_ipv4AddressEntry.insert(std::make_pair(rt.GetDestination(), rt));
  return result.second;
}

bool RoutingTable::Update(RoutingTableEntry& rt)
{
  std::map<Ipv4Address, RoutingTableEntry>::iterator i = 
    m_ipv4AddressEntry.find(rt.GetDestination());
  if(i == m_ipv4AddressEntry.end())
  {
    return false;
  }
  i->second = rt;
  return true;
}

void RoutingTable::DeleteAllRoutesFromInterface(Ipv4InterfaceAddress iface)
{
  if(m_ipv4AddressEntry.empty())
  {
    return;
  }
  for(std::map<Ipv4Address, RoutingTableEntry>::iterator i = m_ipv4AddressEntry.begin();
    i != m_ipv4AddressEntry.end();)
    {
      if(i->second.GetInterface() == iface)
      {
        auto tmp = i;
        ++i;
        m_ipv4AddressEntry.erase(tmp);
      }
      else
      {
        ++i;
      }
    }
}

void RoutingTable::GetListOfAllRoutes(std::map<Ipv4Address, RoutingTableEntry>& allRoutes)
{
  for(auto i = m_ipv4AddressEntry.begin(); i != m_ipv4AddressEntry.end(); ++i)
  {
    if(i->second.GetDestination() != Ipv4Address("127.0.0.1"))
    {
      allRoutes.insert(std::make_pair(i->first, i->second));
    }
  }
}

void RoutingTable::GetListOfDestinationWithNextHop(Ipv4Address nextHop,
                                                  std::map<Ipv4Address, RoutingTableEntry>& unreachable)
{
  unreachable.clear();
  for(auto i = m_ipv4AddressEntry.cbegin(); i != m_ipv4AddressEntry.cend(); ++i)
  {
    if(i->second.GetNextHop() == nextHop)
    {
      unreachable.insert(std::make_pair(i->first, i->second));
    }
  }
}

void RoutingTableEntry::Print(Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
  *stream->GetStream() << std::setiosflags(std::ios::fixed) << m_ipv4Route->GetDestination()
                       << "\t\t" << m_ipv4Route->GetGateway() << "\t\t" << m_iface.GetLocal()
                       << "\t\t" << std::setiosflags(std::ios::left)
                       << std::setprecision(3) << (m_expiryTime - Simulator::Now()).As(unit) << "\n";
}

void RoutingTable::Purge(std::map<Ipv4Address, RoutingTableEntry>& removedAddress)
{
  // No need
  throw;
}

void RoutingTable::Purge()
{
  std::map<Ipv4Address, RoutingTableEntry> outdated;
  for(auto i = m_ipv4AddressEntry.begin(); i != m_ipv4AddressEntry.end(); ++i)
  {
    if((i->second.GetExpiryTime() - Simulator::Now()).GetDouble() <= 0.0)
    {
      outdated.insert(std::make_pair(i->first, i->second));
    }
  }
  for(auto i = outdated.begin(); i != outdated.end(); ++i)
  {
    this->DeleteRoute(i->first);
  }
}

void RoutingTable::Print(Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
  *stream->GetStream()
    << "\nMAQR Routing Table\n"
    << "Destination\t\tGateway\t\tInterface\t\tTime remaining\n";
  for(auto i = m_ipv4AddressEntry.cbegin(); i != m_ipv4AddressEntry.end(); ++i)
  {
    i->second.Print(stream, unit);
  }
  *stream->GetStream() << "\n";
}

bool RoutingTable::AddIpv4Event(Ipv4Address address, EventId id)
{
  auto result = m_ipv4Events.insert(std::make_pair(address, id));
  return result.second;
}

bool RoutingTable::AnyRunningEvent(Ipv4Address address)
{
  EventId event;
  std::map<Ipv4Address, EventId>::const_iterator i = m_ipv4Events.find(address);
  if(m_ipv4Events.empty())
  {
    return false;
  }
  if(i == m_ipv4Events.end())
  {
    return false;
  }
  event = i->second;
  if(event.IsRunning())
  {
    return true;
  }
  else
  {
    return false;
  }
}

bool RoutingTable::ForceDeleteIpv4Event(Ipv4Address address)
{
  EventId event;
  std::map<Ipv4Address, EventId>::const_iterator i = m_ipv4Events.find(address);
  if(m_ipv4Events.empty() || i == m_ipv4Events.end())
  {
    return false;
  }
  event = i->second;
  Simulator::Cancel(event);
  m_ipv4Events.erase(address);
  return true;
}

bool RoutingTable::DeleteIpv4Event(Ipv4Address address)
{
  EventId event;
  std::map<Ipv4Address, EventId>::const_iterator i = m_ipv4Events.find(address);
  if(m_ipv4Events.empty() || i == m_ipv4Events.end())
  {
    return false;
  }
  event = i->second;
  if(event.IsRunning())
  {
    return false;
  }
  if(event.IsExpired())
  {
    event.Cancel();
    m_ipv4Events.erase(address);
    return true;
  }
  else
  {
    m_ipv4Events.erase(address);
    return true;
  }
}

EventId RoutingTable::GetEventId(Ipv4Address address)
{
  std::map<Ipv4Address, EventId>::const_iterator i = m_ipv4Events.find(address);
  if(m_ipv4Events.empty() || i == m_ipv4Events.end())
  {
    return EventId();
  }
  else 
  {
    return i->second;
  }
}

} // namespace maqr
} // namespace ns3