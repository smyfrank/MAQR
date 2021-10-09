//
// Created by smy on 2021/10/2.
//

#include <algorithm>
#include "ns3/log.h"
#include "ns3/wifi-mac-header.h"
#include "maqr-neighbor.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("MaqrNeighbors");

namespace maqr {
Neighbors::Neighbors(Time delay)
  : m_ntimer(Timer::CANCEL_ON_DESTROY)
{
  m_ntimer.SetDelay(delay);
  m_ntimer.SetFunction(&Neighbors::Purge, this);
  m_txErrorCallback = MakeCallback(&Neighbors::ProcessTxError, this);
}

bool Neighbors::IsNeighbor (Ipv4Address addr)
{
  Purge ();
  for(std::vector<Neighbor>::const_iterator i = m_nb.begin(); i != m_nb.end(); ++i)
    {
      if(i->m_neighborAddress == addr)
        {
          return true;
        }
    }
  return false;
}

Time Neighbors::GetExpireTime (Ipv4Address addr)
{
  Purge();
  for(std::vector<Neighbor>::const_iterator i = m_nb.begin(); i != m_nb.end(); ++i)
    {
      if(i->m_neighborAddress == addr)
        {
          return (i->m_expireTime - Simulator::Now());
        }
    }
  return Seconds(0);
}

void Neighbors::SetExpireTime (std::vector<Neighbor>::iterator it, Time expire)
{
  it->m_expireTime = std::max(expire + Simulator::Now(), it->m_expireTime);
}

float Neighbors::GetQueRatio (Ipv4Address addr)
{
  Purge();
  for(std::vector<Neighbor>::const_iterator i = m_nb.begin(); i != m_nb.end(); ++i)
    {
      if(i->m_neighborAddress == addr)
        {
          return i->m_queRatio;
        }
    }
  return -1.0;
}

void Neighbors::SetQueRatio (std::vector<Neighbor>::iterator it, float que)
{
  it->m_queRatio = que;
}

float Neighbors::GetDirection (Ipv4Address addr)
{
  Purge();
  for(std::vector<Neighbor>::const_iterator i = m_nb.begin(); i != m_nb.end(); ++i)
    {
      if(i->m_neighborAddress == addr)
        {
          return i->m_direction;
        }
    }
  return -1.0;
}

void Neighbors::SetDirection (std::vector<Neighbor>::iterator it, float dir)
{
  it->m_direction = dir;
}

float Neighbors::GetSpeed (Ipv4Address addr)
{
  Purge();
  for(std::vector<Neighbor>::const_iterator i = m_nb.begin(); i != m_nb.end(); ++i)
    {
      if(i->m_neighborAddress == addr)
        {
          return i->m_speed;
        }
    }
  return -1.0;
}

void Neighbors::SetSpeed (std::vector<Neighbor>::iterator it, float sp)
{
  it->m_speed = sp;
}

float Neighbors::GetEnergyRatio (Ipv4Address addr)
{
  Purge();
  for(std::vector<Neighbor>::const_iterator i = m_nb.begin(); i != m_nb.end(); ++i)
    {
      if(i->m_neighborAddress == addr)
        {
          return i->m_energyRatio;
        }
    }
  return -1.0;
}

void Neighbors::SetEnergyRatio (std::vector<Neighbor>::iterator it, float energyRatio)
{
  it->m_energyRatio = energyRatio;
}

void Neighbors::UpdateAll (Ipv4Address addr, Time expire, float que, float dir, float sp, float energy)
{
  for(std::vector<Neighbor>::iterator i = m_nb.begin(); i != m_nb.end(); ++i)
    {
      if(i->m_neighborAddress == addr)
        {
          SetExpireTime (i, expire);
          SetQueRatio (i, que);
          SetDirection (i, dir);
          SetSpeed (i, sp);
          SetEnergyRatio (i, energy);
          if(i->m_hardwareAddress == Mac48Address())
            {
              i->m_hardwareAddress = LookupMacAddress (i->m_neighborAddress);
            }
          return;
        }
    }
  NS_LOG_LOGIC ("Open link to " << addr);
  Neighbor neighbor(addr, LookupMacAddress (addr), expire + Simulator::Now(), que,
                     dir, sp, energy);
  m_nb.push_back(neighbor);
  Purge();
}

/**
 * \brief CloseNeighbor structure
 */
struct CloseNeighbor
{
  bool operator() (const Neighbors::Neighbor& nb) const
  {
    return ((nb.m_expireTime < Simulator::Now()) || nb.m_close);
  }
};

void Neighbors::Purge ()
{
  if(m_nb.empty())
    {
      return;
    }

  CloseNeighbor pred;
  if(!m_handleLinkFailure.IsNull())
    {
      for(std::vector<Neighbor>::iterator j = m_nb.begin(); j != m_nb.end(); ++j)
        {
          if(pred(*j))
            {
              NS_LOG_LOGIC("Close link to " << j->m_neighborAddress);
              m_handleLinkFailure(j->m_neighborAddress);
            }
        }
    }
  m_nb.erase(std::remove_if (m_nb.begin(), m_nb.end(), pred), m_nb.end());
  m_ntimer.Cancel();
  m_ntimer.Schedule();
}

void Neighbors::ScheduleTimer ()
{
  m_ntimer.Cancel();
  m_ntimer.Schedule();
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

  for (std::vector<Neighbor>::iterator i = m_nb.begin (); i != m_nb.end (); ++i)
    {
      if (i->m_hardwareAddress == addr)
        {
          i->m_close = true;
        }
    }
  Purge ();
}

} // namespace maqr
} // namespace ns3