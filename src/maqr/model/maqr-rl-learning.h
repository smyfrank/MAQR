#ifndef MAQR_RL_LEARNING
#define MAQR_RL_LEARNING

#include "maqr-neighbor.h"
#include "maqr-packet.h"
#include "ns3/nstime.h"
#include <iomanip>
#include <unordered_set>
#include <cstdlib>
#include <ctime>
#include <algorithm>

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
private:
  Ipv4Address m_destination;
  Time m_lastSeen;
  float m_qValue;
  float m_vValue;
};

class QLearning
{
public:
  QLearning(float alpha, float gamma, float epsilon, Time neighborLifeTime)
  : m_learningRate(alpha),
    m_discoutRate(gamma),
    m_epsilon(epsilon),
    m_neighborReliabilityTimeout(neighborLifeTime)
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
   * \brief Get next hop for target node within active neighbors
   * \param target the target node
   * \param nbList the active neighbors
   * \returns the next hop for the target node
   */
  Ipv4Address GetNextHop (Ipv4Address target, std::unordered_set<Ipv4Address> nbList);
  /**
   * \brief Reward function
   * \param origin the origin address (state)
   * \param hop the hop (action)
   * \returns the reward
   * \TODO: detail design
   */
  float GetReward(Ipv4Address origin, Ipv4Address hop);
  /**
   * \brief Insert new (origin, next hop) entry if not exists
   * \param origin the origin address (state)
   * \param hop the next hop (action)
   */
  void InsertQEntry(Ipv4Address target, Ipv4Address hop, QValueEntry* qEntry);
  /**
   * \brief Purge outdate entries, erase (hop, QEntry*), target still exists
   */
  void Purge();
  

private:
  float m_learningRate;
  float m_discoutRate;
  float m_epsilon;
  Time m_neighborReliabilityTimeout;
  std::map<Ipv4Address, std::map<Ipv4Address, QValueEntry*>> m_QTable;
};

}
}

#endif // MAQR_RL_LEARNING