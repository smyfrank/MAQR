/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef MAQR_HELPER_H
#define MAQR_HELPER_H

#include "ns3/object-factory.h"
#include "ns3/node.h"
#include "ns3/node-container.h"
#include "ns3/ipv4-routing-helper.h"

namespace ns3 {
/**
 * \ingroup aodv
 * \brief Helper class that adds MAQR routing to nodes.
 */
class MaqrHelper : public Ipv4RoutingHelper
{
public:
  MaqrHelper ();

  /**
   * \returns pointer to clone of this MaqrHelper
   * 
   * \internal
   * This method is mainly for internal use by the other helpers;
   * clients are expected to free the dynamic memory allocated by this method
   */
  MaqrHelper* Copy (void) const;

  /**
   * \param node the node on which the routing protocol will run
   * \returns a newly-created routing protocol
   * 
   * This mehod will be called by ns3::InternetStackHelper::Install
   * 
   * \todo support installing MAQR on the subset of all available IP interfaces
   */
  virtual Ptr<Ipv4RoutingProtocol> Create (Ptr<Node> node) const;

  /**
   * \param name the name of the attribute to set
   * \param value the value of the attribute to set
   * 
   * This method controls the attributes of ns3::maqr::RoutingProtocol
   */
  void Set (std::string name, const AttributeValue &value);

  /**
   * Assign a fixed random variable stream number to the random variables
   * used by this model. Return the number of streams (possibly zero) that
   * have been assigned. The Install() method of the InternetStackHelper
   * should have previously been called by the user.
   * 
   * \param stream first stream index to use
   * \param c NodeContainer of the set of nodes for which MAQR
   *          should be modified to use a fixed stream
   * \return the number of stream indices assigned by this helper
   */
  int64_t AssignStreams (NodeContainer c, int64_t stream);

private:
  // the factory to create MAQR routing object
  ObjectFactory m_agentFactory;
};

}

#endif /* MAQR_HELPER_H */

