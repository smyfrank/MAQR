#include "maqr-rl-learning.h"

namespace ns3 {
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
                           << gw->second->GetvValue() << "\t\t" << gw->second->GetSeqNum() << "\t\t"
                           << std::setiosflags(std::ios::left) << std::setprecision(3)
                           << gw->second->GetLastSeen().As(unit) << "\n";
    }
  }
  *stream->GetStream() << "\n";
}

float QLearning::CalculateQValue(Ipv4Address target, Ipv4Address hop)
{
  return (1-m_learningRate) * m_QTable.at(target).at(hop)->GetqValue() + 
          m_learningRate * (m_discoutRate * m_QTable.at(target).at(hop)->GetvValue());
}

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
          res = CalculateQValue(target, act->first);
        }
        res = std::max(res, CalculateQValue(target, act->first));
      }
    }
  }
  return res;
}

Ipv4Address QLearning::GetNextHop(Ipv4Address target)
{
  Ipv4Address a = Ipv4Address::GetZero();
  float res = -100000;
  if(m_QTable.find(target) != m_QTable.end())
  {
    for(auto act = m_QTable.find(target)->second.begin(); act != m_QTable.find(target)->second.end();)
    {
      float deltaT = (Simulator::Now() - act->second->GetLastSeen()).GetSeconds();
      if(deltaT <= m_neighborReliabilityTimeout.GetSeconds())
      {
        if(act == m_QTable.find(target)->second.begin())
        {
          res = CalculateQValue(target, act->first);
          a = act->first;
        }
        else if(CalculateQValue(target, act->first) > res)
        {
          res = CalculateQValue(target, act->first);
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

float QLearning::GetReward(Ipv4Address origin, Ipv4Address hop)
{
  return 0.0;
}

} // namespace maqr
} // namespace ns3