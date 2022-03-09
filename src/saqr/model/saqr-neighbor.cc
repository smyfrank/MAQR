//
// Created by smy on 2021/10/2.
//

#include <algorithm>
#include "ns3/log.h"
#include "ns3/wifi-mac-header.h"
#include "saqr-neighbor.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("SaqrNeighbors");

namespace saqr {
Neighbors::Neighbors(Time delay)
  : m_entryLifeTime(delay)
{
  m_txErrorCallback = MakeCallback(&Neighbors::ProcessTxError, this);
}

Time Neighbors::GetEntryUpdateTime (Ipv4Address ip)
{
  if (ip == Ipv4Address::GetZero ())
  {
    return Time (Seconds (0));
  }
  auto i = m_nbTable.find(ip);
  return i->second.m_updatedTime;
}

void Neighbors::AddEntry (Ipv4Address ip, Neighbor nb)
{
  auto i = m_nbTable.find(ip);
  if(i != m_nbTable.end() || ip == i->first)
  {
    m_nbTable.erase(ip);
    m_nbTable.insert(std::make_pair(ip, Neighbor(nb)));
    return;
  }
  m_nbTable.insert(std::make_pair(ip, Neighbor(nb)));
}

void Neighbors::DeleteEntry(Ipv4Address ip)
{
  m_nbTable.erase(ip);
}

Vector2D Neighbors::GetPosition(Ipv4Address ip)
{
  auto i = m_nbTable.find(ip);
  if(i != m_nbTable.end())
  {
    return i->second.m_position;
  }
  return GetInvalidPosition();
}

float Neighbors::GetQueueRatio(Ipv4Address ip)
{
  auto i = m_nbTable.find(ip);
  if(i != m_nbTable.end())
  {
    return i->second.m_queRatio;
  }
  return -1.0;
}

float Neighbors::GetDirection(Ipv4Address ip)
{
  auto i = m_nbTable.find(ip);
  if(i != m_nbTable.end())
  {
    return i->second.m_direction;
  }
  return -1.0;
}

float Neighbors::GetSpeed(Ipv4Address ip)
{
  auto i = m_nbTable.find(ip);
  if(i != m_nbTable.end())
  {
    return i->second.m_speed;
  }
  return -1.0;
}

bool Neighbors::IsNeighbor(Ipv4Address ip)
{
  auto i = m_nbTable.find(ip);
  if(i != m_nbTable.end() || ip == i->first)
  {
    return true;
  }
  return false;
}

void Neighbors::Purge()
{
  if(m_nbTable.empty())
  {
    return;
  }
  std::list<Ipv4Address> toErase;
  for(auto i = m_nbTable.begin(); i != m_nbTable.end(); ++i)
  {
    if(m_entryLifeTime + GetEntryUpdateTime(i->first) <= Simulator::Now())
    {
      toErase.insert(toErase.begin(), i->first);
    }
  }
  toErase.unique();

  for(auto i = toErase.begin(); i != toErase.end(); ++i)
  {
    m_nbTable.erase(*i);
  }
}

void Neighbors::Clear()
{
  m_nbTable.clear();
}

std::set<Ipv4Address> Neighbors::GetAllActiveNeighbors ()
{
  std::set<Ipv4Address> activeNeighbors;
  Purge ();
  for (auto i = m_nbTable.cbegin (); i != m_nbTable.cend (); i++)
  {
    activeNeighbors.insert (i->first);
  }
  return activeNeighbors;
}







void Neighbors::AddArpCache (Ptr<ArpCache> a)
{
  m_arp.push_back(a);
}

void Neighbors::DelArpCache (Ptr<ArpCache> a)
{
  m_arp.erase (std::remove (m_arp.begin (), m_arp.end (), a), m_arp.end ());
}

Mac48Address Neighbors::LookupMacAddress (Ipv4Address addr)
{
  Mac48Address hwaddr;
  for (std::vector<Ptr<ArpCache> >::const_iterator i = m_arp.begin ();
       i != m_arp.end (); ++i)
    {
      ArpCache::Entry * entry = (*i)->Lookup (addr);
      if (entry != 0 && (entry->IsAlive () || entry->IsPermanent ()) && !entry->IsExpired ())
        {
          hwaddr = Mac48Address::ConvertFrom (entry->GetMacAddress ());
          break;
        }
    }
  return hwaddr;
}

void Neighbors::ProcessTxError (WifiMacHeader const & hdr)
{
  Mac48Address addr = hdr.GetAddr1 ();

  for (auto i = m_nbTable.begin(); i != m_nbTable.end (); ++i)
    {
      if (i->second.m_hardwareAddress == addr)
        {
          i->second.m_close = true;
        }
    }
  Purge ();
}

} // namespace saqr
} // namespace ns3