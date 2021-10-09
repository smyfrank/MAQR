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
   Neighbors (Time delay);
   /// Neighbor description
   struct Neighbor
   {
     /// Neighbor IPv4 address
     Ipv4Address m_neighborAddress;
     /// Neighbor MAC address
     Mac48Address m_hardwareAddress;
     /// Neighbor expire time
     Time m_expireTime;
     /// Neighbor close indicator
     bool m_close;
     /// Neighbor queue length
     float m_queRatio;
     /// Neighbor moving direction
     float m_direction;
     /// Neighbor moving speed
     float m_speed;
     /// Neighbor energy ratio
     float m_energyRatio;

     /**
      * \brief Neighbor structure constructor
      * \param ip Ipv4Address entry
      * \param mac Mac48Address entry
      * \param t Time expire time
      * \param queue
      * \param direction
      * \param speed
      * \param energy
      */
      Neighbor (Ipv4Address ip, Mac48Address mac, Time t, float queue,
               float direction, float speed, float energy) :
           m_neighborAddress(ip),
           m_hardwareAddress(mac),
           m_expireTime(t),
           m_close(false),
           m_queRatio(queue),
           m_direction(direction),
           m_speed(speed),
           m_energyRatio(energy)
      {

      }
   };
   /**
   * Return expire time for neighbor node with address addr, if exists, else return 0.
   * \param addr the IP address of the neighbor node
   * \returns the expire time for the neighbor node
   */
   Time GetExpireTime (Ipv4Address addr);
   /**
   * Check that node with address addr is neighbor
   * \param addr the IP address to check
   * \returns true if the node with IP address is a neighbor
   */
   bool IsNeighbor (Ipv4Address addr);

   void SetExpireTime (std::vector<Neighbor>::iterator it, Time expire);

   // TODO: calculation of queue, direction, speed, energy.
   float GetQueRatio(Ipv4Address addr);

   void SetQueRatio(std::vector<Neighbor>::iterator it, float que);

   float GetDirection(Ipv4Address addr);

   void SetDirection(std::vector<Neighbor>::iterator it, float dir);

   float GetSpeed(Ipv4Address addr);

   void SetSpeed(std::vector<Neighbor>::iterator it, float sp);

   float GetEnergyRatio(Ipv4Address addr);

   void SetEnergyRatio(std::vector<Neighbor>::iterator it, float energyRatio);

   void UpdateAll(Ipv4Address addr, Time expire, float que, float dir, float sp, float energy);
   /// Remove all expired entries
   void Purge ();
   /// Schedule m_ntimer.
   void ScheduleTimer ();
   /// Remove all entries
   void Clear ()
   {
     m_nb.clear ();
   }

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
  Timer m_ntimer;
  /// vector of entries
  std::vector<Neighbor> m_nb;
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
