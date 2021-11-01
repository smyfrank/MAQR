#include "maqr-rl-learning.h"

namespace ns3 {
namespace maqr {

float QLearning::CalculateQValue(Ipv4Address target, Ipv4Address hop)
{
  return (1-m_learningRate) * m_QTable.at(target).at(hop)->GetqValue() + 
          m_learningRate * (m_discoutRate * m_QTable.at(target).at(hop)->GetvValue());
}

}
}