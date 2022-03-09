#ifndef MAQR_RL_LEARNING
#define MAQR_RL_LEARNING

#include "maqr-neighbor.h"
#include "maqr-packet.h"
#include "ns3/nstime.h"
#include "ns3/node-list.h"
#include "ns3/ipv4.h"
#include <iomanip>
#include <set>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <numeric>
#include <random>

namespace ns3 {
namespace maqr {

enum RewardType
{
  REACH_DESTINATION,
  VOID_AREA,
  LOOP,
  MIDWAY
};

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
  QLearning(float alpha, float gamma, float epsilon, float epsilonLimit, float decayRate, bool updateEpsilon, Time neighborLifeTime)
  : m_learningRate(alpha),
    m_discoutRate(gamma),
    m_epsilon(epsilon),
    m_epsilonLowerLimit(epsilonLimit),
    m_decayRate(decayRate),
    m_updateEpsilon(updateEpsilon),
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
   * \TODO:
   */
  // virtual float UpdateQValue(Ipv4Address target, Ipv4Address hop, RewardType type);
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
  virtual Ipv4Address GetNextHop (Ipv4Address target, const std::set<Ipv4Address>& nbList);
  /**
   * \brief Reward function
   * \param origin the origin address (state)
   * \param hop the hop (action)
   * \returns the reward
   * \TODO: detail design
   */
  float GetReward(Ipv4Address hop, RewardType type);
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
  // Get node with Ipv4Address
  Ptr<Node> GetNodeWithAddress (Ipv4Address address);
  // Get max Q value from neighbor to destination
  // float GetMaxNextStateQValue (Ipv4Address hop, Ipv4Address target);

  // Decay epsilon in each step
  void DecayEpsilon ();

  float m_learningRate;
  float m_discoutRate;
  float m_epsilon;
  float m_epsilonLowerLimit;
  float m_decayRate;
  bool m_updateEpsilon;
  Time m_neighborReliabilityTimeout;
  std::map<Ipv4Address, std::map<Ipv4Address, QValueEntry*>> m_QTable;
};

class MultiAgentQLearning : public QLearning
{
public:
  MultiAgentQLearning (float alpha, float gamma, float epsilon, float epsilonLimit, float decayRate,
                       bool updateEpsilon, Time neighborLifeTime, float deltaWin, float deltaLose, int nnodes)
  : QLearning (alpha, gamma, epsilon, epsilonLimit, decayRate, updateEpsilon, neighborLifeTime),
    m_deltaWin (deltaWin),
    m_deltaLose (deltaLose),
    m_nnodes (nnodes)
  {
  }

  ~MultiAgentQLearning ()
  {
  }
  // initialization
  void Init (const std::set<Ipv4Address>& allNodes);
  // init counter
  void GenerateCounter (const std::set<Ipv4Address>& allNodes);
  // init average estimation strategy table and strategy table
  void GenerateSrategyTable (const std::set<Ipv4Address>& allNodes);
  // Generate Q-table
  void GenerateQTable (const std::set<Ipv4Address>& allNodes);
  // Get next hop (act)
  virtual Ipv4Address GetNextHop (Ipv4Address dst, const std::set<Ipv4Address>& nbList);
  // Update Q-table, strategy table, average estimation strategy table
  void Learn (Ipv4Address dst, Ipv4Address hop, RewardType rewardType, float maxNextQ);
  // Choose m_deltaWin or m_deltaLose
  float ChooseDelta (Ipv4Address dst);
  // update strategy table
  void UpdateStrategy (Ipv4Address dst);
  // update average strategy table
  void UpdateAvgStrategy (Ipv4Address dst);

  float m_deltaWin;
  float m_deltaLose;
  std::map<Ipv4Address, std::map<Ipv4Address, float>> m_avgStrategy;
  std::map<Ipv4Address, std::map<Ipv4Address, float>> m_strategy;
  std::map<Ipv4Address, int> m_counter;
  std::map<Ipv4Address, std::map<Ipv4Address, float>> m_multiQTable;
  int m_nnodes;
};

}
}

#endif // MAQR_RL_LEARNING