#include "maqr-rl-learning.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MaqrRlLearning");

namespace maqr {

void QLearning::PrintQTable(Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
  *stream->GetStream() << "MAQR Q-Table at " << Simulator::Now().As(unit) << "\n"
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

float QLearning::GetReward(Ipv4Address hop, RewardType type, float mobFactor)
{
  // TODO
  NS_LOG_FUNCTION (this);
  switch (type)
  {
    case REACH_DESTINATION:
      return 46;
    case VOID_AREA:
      return -20;
    case LOOP:
      return -20;
    case MIDWAY:
      return mobFactor;
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

void MultiAgentQLearning::Init (const std::set<Ipv4Address>& allNodes)
{
  NS_LOG_FUNCTION (this);
  GenerateCounter (allNodes);
  GenerateSrategyTable (allNodes);
  GenerateQTable (allNodes);
  return;
}

void MultiAgentQLearning::GenerateCounter (const std::set<Ipv4Address>& allNodes)
{
  NS_LOG_FUNCTION (this);
  for (const auto& i : allNodes)
  {
    m_counter[i] = 0;
  }
  return;
}

void MultiAgentQLearning::GenerateSrategyTable (const std::set<Ipv4Address>& allNodes)
{
  NS_LOG_FUNCTION (this);
  int nnodes = allNodes.size ();
  for (const auto& dst : allNodes)
  {
    for (const auto& hop : allNodes)
    {
      if (dst != hop)
      {
        m_avgStrategy[dst][hop] = 1.0 / (nnodes - 1);
        m_strategy[dst][hop] = 1.0 / (nnodes - 1);
      }
      else
      {
        m_avgStrategy[dst][hop] = 0.0;
        m_strategy[dst][hop] = 0.0;
      }
    }
  }
  return;
}

void MultiAgentQLearning::GenerateQTable (const std::set<Ipv4Address>& allNodes)
{
  NS_LOG_FUNCTION (this);
  for (const auto& dst : allNodes)
  {
    for (const auto& hop : allNodes)
    {
      m_multiQTable[dst][hop] = 0;
    }
  }
}

Ipv4Address MultiAgentQLearning::GetNextHop (Ipv4Address dst, const std::set<Ipv4Address>& nbList)
{
  if (m_updateEpsilon)
  {
    DecayEpsilon ();
  }

  // if dst is one of neighbors
  if (nbList.find (dst) != nbList.end ())
  {
    return dst;
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

  // choose next hop by weights (sum of prefix and binary search)
  std::map<int, Ipv4Address> idxToAddr;
  std::vector<float> preSum;
  int index = 0;
  for (auto i = m_strategy[dst].begin (); i != m_strategy[dst].end (); ++i)
  {
    if (nbList.find (i->first) != nbList.end ())
    {
      if (index == 0)
      {
        preSum.push_back (i->second);
        idxToAddr[index] = i->first;
        index++;
      }
      else
      {
        preSum.push_back (preSum[index - 1] + i->second);
        idxToAddr[index] = i->first;
        index++;
      }
    }
  }
  std::random_device rd;
  std::default_random_engine eng {rd()};
  std::uniform_real_distribution<double> dis(0, preSum.back ());
  double tmp = dis (eng);
  int pos = std::lower_bound (preSum.begin (), preSum.end (), tmp) - preSum.begin ();
  Ipv4Address act = idxToAddr[pos];

  return act;
}

void MultiAgentQLearning::Learn (Ipv4Address dst, Ipv4Address hop, RewardType rewardType, float maxNextQ, float mobFactor)
{
  NS_LOG_FUNCTION (this << "learn dst " << dst << " hop " << hop << " rewardType " << rewardType);
  if (hop == Ipv4Address ("102.102.102.102"))
  {
    NS_LOG_LOGIC ("No hop was executed");
    return;
  }

  // update Q table
  m_multiQTable[dst][hop] = m_multiQTable[dst][hop] + m_learningRate * (GetReward (hop, rewardType, mobFactor) + m_discoutRate * maxNextQ - m_multiQTable[dst][hop]);
  // update average strategy
  UpdateAvgStrategy (dst);
  // update strategy
  UpdateStrategy (dst);

  return;
}

float MultiAgentQLearning::ChooseDelta (Ipv4Address dst)
{
  float stratSum = 0.0, meanStratSum = 0.0;
  for (auto i = m_strategy[dst].begin (); i != m_strategy[dst].end (); ++i)
  {
    stratSum += i->second * m_multiQTable[dst][i->first];
    meanStratSum += m_avgStrategy[dst][i->first] * m_multiQTable[dst][i->first];
  }

  return stratSum > meanStratSum ? m_deltaWin : m_deltaLose;
}

void MultiAgentQLearning::UpdateStrategy (Ipv4Address dst)
{
  // find the action with the highest Q-value
  Ipv4Address maxQIdx = m_multiQTable[dst].begin()->first;
  float maxQ = m_multiQTable[dst].begin ()->second;
  for (const auto& i : m_multiQTable[dst])
  {
    if (i.second > maxQ)
    {
      maxQIdx = i.first;
      maxQ = i.second;
    }
  }

  // update strategy table
  for (auto i = m_strategy[dst].begin (); i != m_strategy[dst].end (); ++i)
  {
    float dPlus = ChooseDelta (dst);
    float dMinus = (-1.0) * dPlus / ((m_nnodes - 1) - 1.0);
    if (i->first == maxQIdx)
    {
      i->second = std::min (float(1.0), i->second + dPlus);
    }
    else
    {
      i->second = std::max (float(0.0), i->second + dMinus);
    }
  }
  return;
}

void MultiAgentQLearning::UpdateAvgStrategy (Ipv4Address dst)
{
  m_counter[dst] += 1;
  for (auto i = m_avgStrategy[dst].begin (); i != m_avgStrategy[dst].end (); ++i)
  {
    i->second += 1.0 / m_counter[dst] * (m_strategy[dst][i->first] - i->second);
  }
  return;
}

} // namespace maqr
} // namespace ns3