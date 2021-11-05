#ifndef MAQR_RL_LEARNING
#define MAQR_RL_LEARNING

#include "maqr-neighbor.h"
#include "maqr-packet.h"
#include "ns3/nstime.h"
#include <iomanip>

namespace ns3 {
namespace maqr {

class QValueEntry
{
public:
  QValueEntry(Ipv4Address dest)
  : m_destination(dest)
  {
  }
  ~QValueEntry()
  {
  }

  void SetLastSeen(Time t)
  {
    m_lastSeen = t;
  }
  void SetqValue(float v)
  {
    m_qValue = v;
  }
  void SetvValue(float v)
  {
    m_vValue = v;
  }
  void SetSeqNum(uint32_t seq)
  {
    m_seqNum = seq;
  }
  Ipv4Address GetDestination() const
  {
    return m_destination;
  }
  Time GetLastSeen() const
  {
    return m_lastSeen;
  }
  float GetqValue() const
  {
    return m_qValue;
  }
  float GetvValue() const
  {
    return m_vValue;
  }
  uint32_t GetSeqNum() const
  {
    return m_seqNum;
  }
private:
  Ipv4Address m_destination;
  Time m_lastSeen;
  float m_qValue;
  float m_vValue;
  uint32_t m_seqNum;
};

class QLearning
{
public:
  QLearning(float alpha, float gamma, float epsilon, 
            Neighbors nb, std::map<Ipv4Address, std::map<Ipv4Address, QValueEntry*>> qt)
  : m_learningRate(alpha),
    m_discoutRate(gamma),
    m_epsilon(epsilon),
    m_neighbors(nb),
    m_QTable(qt)
  {
  }
  virtual ~QLearning()
  {
  }
  /**
   * \brief Print the Q Table
   * \param stream the output stream wrapper
   * \param unit the time unit
   */
  virtual void PrintQTable(Ptr<OutputStreamWrapper> stream, Time::Unit unit) const;
  /**
   * \brief Calculate the Q Value
   * \param target the target node(state)
   * \param hop the candidate hop(action)
   * \returns the Q Value
   */
  virtual float CalculateQValue(Ipv4Address target, Ipv4Address hop);
  /**
   * \brief Get max Q value for target node(only search in the active neighbors)
   * \param target the target node
   * \returns the best Q value for target node
   */
  virtual float GetMaxValue(Ipv4Address target);
  /**
   * \brief Get best next hop for target node
   * \param target the target node
   * \returns the best next hop for the target node
   */
  Ipv4Address GetNextHop(Ipv4Address target);
  /**
   * \brief Reward function
   * \param origin the origin address (state)
   * \param hop the hop (action)
   * \returns the reward
   * \TODO: detail design
   */
  float GetReward(Ipv4Address origin, Ipv4Address hop);
  

private:
  float m_learningRate;
  float m_discoutRate;
  float m_epsilon;
  Time m_neighborReliabilityTimeout;
  Neighbors m_neighbors;
  std::map<Ipv4Address, std::map<Ipv4Address, QValueEntry*>> m_QTable;
};

}
}

#endif // MAQR_RL_LEARNING