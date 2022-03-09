#include "saqr-rl-learning.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("SaqrRlLearning");

namespace saqr {

void QLearning::PrintQTable(Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
  *stream->GetStream() << "SAQR Q-Table at " << Simulator::Now().As(unit) << "\n"
                       << "Destination\t\tGateway\t\tQ\t\tV\t\tSEQ\t\tLastSeen\n";
  for(auto dst = m_QTable.cbegin(); dst != m_QTable.cend(); ++dst)
  {
    for(auto gw = dst->second.cbegin(); gw != dst->second.cend(); ++gw)
    {
      *stream->GetStream() << std::setiosflags(std::ios::fixed) << dst->first << "\t\t"
                           << gw->first << "\t\t" << gw->second->GetqValue() << "\t\t"
                           << gw->second->GetvValue() << "\t\t"
                           << std::setiosflags(std::ios::left) << std::setprecision(3)
                           << gw->second->GetLastSeen().As(unit) << "\n";
    }
  }
  *stream->GetStream() << "\n";
}

/*
float QLearning::UpdateQValue(Ipv4Address target, Ipv4Address hop, RewardType type)
{
  // First get max Q-value amont the actions from hop to target
  float maxQ = GetMaxNextStateQValue (hop, target);

  float newQValue = (1-m_learningRate) * m_QTable.at(target).at(hop)->GetqValue() + 
          m_learningRate * (GetReward(hop, type) + m_discoutRate * maxQ);
  m_QTable.find (target)->second.find (hop)->second->SetqValue (newQValue);
  return newQValue;
}
*/

float QLearning::GetMaxValue(Ipv4Address target)
{
  float res = -100000;
  if(m_QTable.find(target) != m_QTable.end())
  {
    for(auto act = m_QTable.find(target)->second.begin(); act != m_QTable.find(target)->second.end(); ++act)
    {
      float deltaT = (Simulator::Now() - act->second->GetLastSeen()).GetSeconds();
      if(deltaT <= m_neighborReliabilityTimeout.GetSeconds())
      {
        if(act == m_QTable.find(target)->second.begin())
        {
          res = m_QTable.find (target)->second.cbegin ()->second->GetqValue ();
        }
        res = std::max(res, m_QTable.find (target)->second.cbegin ()->second->GetqValue ());
      }
    }
  }
  return res;
}

Ipv4Address QLearning::GetNextHop(Ipv4Address target)
{
  Ipv4Address a = Ipv4Address::GetZero();
  float res = -100000.0;
  if(m_QTable.find(target) != m_QTable.end())
  {
    for(auto act = m_QTable.find(target)->second.begin(); act != m_QTable.find(target)->second.end();)
    {
      float deltaT = (Simulator::Now() - act->second->GetLastSeen()).GetSeconds();
      if(deltaT <= m_neighborReliabilityTimeout.GetSeconds())
      {
        if(act == m_QTable.find(target)->second.begin())
        {
          res = m_QTable.find (target)->second.cbegin ()->second->GetqValue ();
          a = act->first;
        }
        else if(m_QTable.find (target)->second.cbegin ()->second->GetqValue () > res)
        {
          res = m_QTable.find (target)->second.cbegin ()->second->GetqValue ();
          a = act->first;
        }
        act++;
      }
      else
      {
        ///TODO:neighbor node is not active
        delete act->second;
        act = m_QTable.at(target).erase(act);
      }
    }
  }
  return a;
}

Ipv4Address QLearning::GetNextHop (Ipv4Address target, const std::set<Ipv4Address>& nbList)
{  
  // decay epsilon
  if (m_updateEpsilon)
  {
    DecayEpsilon();
  }

  // New destination entry
  if (m_QTable.find (target) == m_QTable.end ())
  {
    m_QTable.insert (std::make_pair (target, std::map<Ipv4Address, QValueEntry*> ()));
  }

  // Insert new actions into corresponding destination or unpdate last seen time
  for (auto i = nbList.cbegin (); i != nbList.cend (); i++)
  {
    if (m_QTable.find (target)->second.find (*i) == m_QTable.find (target)->second.end ())
    {
      QValueEntry* qEntry = new QValueEntry (target);
      qEntry->SetLastSeen (Simulator::Now ());
      qEntry->SetqValue (0.0);
      qEntry->SetvValue (0.0);
      m_QTable.find (target)->second.insert (std::make_pair (*i, qEntry));
    }
    else
    {
      m_QTable.find (target)->second.find (*i)->second->SetLastSeen (Simulator::Now ());
    }
  }

  if (nbList.find (target) != nbList.end ())
  {
    return target;
  }

  // exploration, randowly choose a neighbor as next hop with probability epsilon
  srand (time (NULL));
  float prob = (rand () % (1000)) / 1000.0;
  if (prob < m_epsilon)
  {
    auto i = nbList.cbegin ();
    std::advance (i, rand () % nbList.size ());
    return *i;
  }

  // exploitation choose the neighbor with highest Q-value
  Ipv4Address a = Ipv4Address::GetZero ();
  float res = 0;
  if (m_QTable.find (target) != m_QTable.end ())
  {
    for (auto candidate = m_QTable.find (target)->second.begin (); candidate != m_QTable.find (target)->second.end (); ++candidate)
    {
      if (nbList.find (candidate->first) != nbList.end ())
      {
        if (candidate->second->GetqValue () > res)
        {
          res = candidate->second->GetqValue ();
          a = candidate->first;
        }
      }
    }
  }

  if (a == Ipv4Address::GetZero ())
  {
    auto i = nbList.cbegin ();
    std::advance (i, rand () % nbList.size ());
    return *i;
  }

  return a;
}

float QLearning::GetReward(Ipv4Address hop, RewardType type)
{
  // TODO
  NS_LOG_FUNCTION (this);
  switch (type)
  {
    case REACH_DESTINATION:
      return 10;
    case VOID_AREA:
      return -10;
    case LOOP:
      return -10;
    case MIDWAY:
      return 1;
  }
  return 0;
}

void QLearning::InsertQEntry(Ipv4Address target, Ipv4Address hop, QValueEntry* qEntry)
{
  if(m_QTable.find(target) != m_QTable.end())
  {
    auto i = m_QTable.find(target);
    if(i->second.find(hop) != i->second.end())
    {
      return;
    }
    else
    {
      i->second.insert(std::make_pair(hop, qEntry));
    }
  }
  else
  {
    m_QTable.insert(std::make_pair(target, std::map<Ipv4Address, QValueEntry*>()));
    m_QTable[target].insert(std::make_pair(hop, qEntry));
  }
}

void QLearning::Purge()
{
  std::vector<Ipv4Address> purgeVec;
  for(auto i = m_QTable.begin(); i != m_QTable.end(); ++i)
  {
    for(auto j = i->second.begin(); j != i->second.end(); ++j)
    {
      if((Simulator::Now() - j->second->GetLastSeen()).GetSeconds() 
      > m_neighborReliabilityTimeout.GetSeconds())
      {
        if(j->second != nullptr)
        {
          delete j->second;
          j->second = nullptr;
        }
        purgeVec.emplace_back(j->first);
      }
    }

    for(auto j = purgeVec.begin(); j != purgeVec.end(); ++j)
    {
      i->second.erase(*j);
    }

    purgeVec.clear();
  }
}

Ptr<Node> QLearning::GetNodeWithAddress (Ipv4Address address)
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

void QLearning::DecayEpsilon ()
{
  if (m_epsilon > m_epsilonLowerLimit)
  {
    m_epsilon *= m_decayRate;
  }
}

} // namespace saqr
} // namespace ns3