//
// Created by smy on 2021/10/2.
//

#ifndef MAQR_NEIGHBOR_H
#define MAQR_NEIGHBOR_H

#include <vector>
#include "ns3/simulator.h"
#include "ns3/timer.h"
#include "ns3/ipv4-address.h"
#include "ns3/callback.h"
#include "ns3/arp-cache.h"
#include "ns3/mobility-module.h"

namespace ns3 {

class WifiMacHeader;

namespace maqr {

class RoutingProtocol;

/**
 * \ingroup maqr
 * \brief maintain list of active neighbors
 */
class Neighbors
{
public:
  /**
   * \brief constructor
   * \param delay the delay time for purging the list of neighbors
   */
   Neighbors (Time delay = Seconds(1));
   /// Neighbor description
   struct Neighbor
   {
     /// Neighbor IPv4 address
     Ipv4Address m_neighborAddress;
     /// Neighbor MAC address
     Mac48Address m_hardwareAddress;
     /// Neighbor expire time
     Time m_updatedTime;
     /// Neighbor close indicator
     bool m_close;
     // Neighbor position
     Vector2D m_position;
     /// Neighbor queue length
     float m_queRatio;
     /// Neighbor moving direction
     float m_direction;
     /// Neighbor moving speed
     float m_speed;

     /**
      * \brief Neighbor structure constructor
      * \param ip Ipv4Address entry
      * \param mac Mac48Address entry
      * \param t Time expire time
      * \param queue
      * \param direction
      * \param speed
      */
      Neighbor (Ipv4Address ip, Mac48Address mac, Time t, Vector2D pos, float queue,
               float direction, float speed) :
           m_neighborAddress(ip),
           m_hardwareAddress(mac),
           m_updatedTime(t),
           m_close(false),
           m_position(pos),
           m_queRatio(queue),
           m_direction(direction),
           m_speed(speed)
      {

      }
   };

  /**
   * \brief Gets the last time the entry was updated
   * \param id Ipv4Address to get time of update from
   * \return Time of last update to the neighbor
   */
  Time GetEntryUpdateTime (Ipv4Address id);
  /**
   * \brief Adds entry in the neighbor table
   */
  void AddEntry (Ipv4Address ip, Neighbor nb);
  /**
   * \brief Deletes entry in position table
   */
  void DeleteEntry (Ipv4Address ip);
  /**
   * \brief Gets position from neighbor table
   */
  Vector2D GetPosition(Ipv4Address ip);

  Vector2D GetInvalidPosition()
  {
    return Vector2D(-1, -1);
  }

  /**
   * \brief Gets queue ratio of ip
   */
  float GetQueueRatio(Ipv4Address ip);
  /**
   * \brief Gets direction of neighbor ip
   */
  float GetDirection(Ipv4Address ip);
  /**
   * \brief Gets absolute speed of neighbor ip
   */
  float GetSpeed(Ipv4Address ip);
  
  /**
   * \brief Checks if a node is a neighbor
   */
  bool IsNeighbor(Ipv4Address ip);
  /**
   * \brief Remove entries with expired lieftime
   */
  void Purge();
  /**
   * \brief clears all entries
   */
  void Clear();
  


   /**
   * Add ARP cache to be used to allow layer 2 notifications processing
   * \param a pointer to the ARP cache to add
   */
   void AddArpCache (Ptr<ArpCache> a);
   /**
   * Don't use given ARP cache any more (interface is down)
   * \param a pointer to the ARP cache to delete
   */
   void DelArpCache (Ptr<ArpCache> a);
   /**
   * Get callback to ProcessTxError
   * \returns the callback function
   */
   Callback<void, WifiMacHeader const &> GetTxErrorCallback () const
   {
     return m_txErrorCallback;
   }

   /**
   * Set link failure callback
   * \param cb the callback function
   */
   void SetCallback (Callback<void, Ipv4Address> cb)
   {
     m_handleLinkFailure = cb;
   }
   /**
   * Get link failure callback
   * \returns the link failure callback
   */
   Callback<void, Ipv4Address> GetCallback () const
   {
     return m_handleLinkFailure;
   }

private:
  /// link failure callback
  Callback<void, Ipv4Address> m_handleLinkFailure;
  /// TX error callback
  Callback<void, WifiMacHeader const &> m_txErrorCallback;
  /// Timer for neighbor's list. Schedule Purge().
  Time m_entryLifeTime;
  /// vector of entries
  std::map<Ipv4Address, Neighbor> m_nbTable;
  /// list of ARP cached to be used for layer 2 notifications processing
  std::vector<Ptr<ArpCache>> m_arp;

  /**
   * Find MAC address by IP using list of ARP caches
   *
   * \param addr the IP address to lookup
   * \returns the MAC address for the IP address
   */
  Mac48Address LookupMacAddress (Ipv4Address addr);
  /**
   * Process layer 2 TX error notification
   * \param hdr header of the packet
   */
  void ProcessTxError (WifiMacHeader const &hdr);
};

} // namespace maqr
} // namespace ns3

#endif //MAQR_NEIGHBOR_H
